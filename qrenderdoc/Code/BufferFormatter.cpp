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

#include <QApplication>
#include <QRegularExpression>
#include <QtMath>
#include "QRDUtils.h"

struct StructFormatData
{
  // the actual definition (including structs pulled in recursively)
  ShaderConstant structDef;
  // the line in the format where each member was defined, in case we find a problem later and want
  // to attach the error to it
  QList<int> lineMemberDefs;

  uint32_t pointerTypeId = 0;
  uint32_t offset = 0;
  uint32_t alignment = 0;
  uint32_t paddedStride = 0;

  // is this a struct definition with [[single]] ?
  bool singleDef = false;

  // does this contain a member annotated with [[single]] ?
  bool singleMember = false;

  bool signedEnum = false;
};

GraphicsAPI BufferFormatter::m_API;

static bool MatchBaseTypeDeclaration(QString basetype, const bool isUnsigned, ShaderConstant &el)
{
  if(basetype == lit("bool"))
  {
    el.type.baseType = VarType::Bool;
  }
  else if(basetype == lit("byte") || basetype == lit("char") || basetype == lit("int8_t"))
  {
    el.type.baseType = VarType::SByte;

    if(isUnsigned)
      el.type.baseType = VarType::UByte;
  }
  else if(basetype == lit("ubyte") || basetype == lit("xbyte") || basetype == lit("uint8_t"))
  {
    el.type.baseType = VarType::UByte;
  }
  else if(basetype == lit("short") || basetype == lit("int16_t"))
  {
    el.type.baseType = VarType::SShort;

    if(isUnsigned)
      el.type.baseType = VarType::UShort;
  }
  else if(basetype == lit("ushort") || basetype == lit("xshort") || basetype == lit("uint16_t"))
  {
    el.type.baseType = VarType::UShort;
  }
  else if(basetype == lit("long") || basetype == lit("int64_t"))
  {
    el.type.baseType = VarType::SLong;

    if(isUnsigned)
      el.type.baseType = VarType::ULong;
  }
  else if(basetype == lit("ulong") || basetype == lit("xlong") || basetype == lit("uint64_t"))
  {
    el.type.baseType = VarType::ULong;
  }
  else if(basetype == lit("int") || basetype == lit("ivec") || basetype == lit("imat") ||
          basetype == lit("int32_t"))
  {
    el.type.baseType = VarType::SInt;

    if(isUnsigned)
      el.type.baseType = VarType::UInt;
  }
  else if(basetype == lit("uint") || basetype == lit("xint") || basetype == lit("uvec") ||
          basetype == lit("umat") || basetype == lit("uint32_t"))
  {
    el.type.baseType = VarType::UInt;
  }
  else if(basetype == lit("half") || basetype == lit("float16_t"))
  {
    el.type.baseType = VarType::Half;
  }
  else if(basetype == lit("float") || basetype == lit("vec") || basetype == lit("mat") ||
          basetype == lit("float32_t"))
  {
    el.type.baseType = VarType::Float;
  }
  else if(basetype == lit("double") || basetype == lit("dvec") || basetype == lit("dmat") ||
          basetype == lit("float64_t"))
  {
    el.type.baseType = VarType::Double;
  }
  else if(basetype == lit("unormh"))
  {
    el.type.baseType = VarType::UShort;
    el.type.flags |= ShaderVariableFlags::UNorm;
  }
  else if(basetype == lit("unormb"))
  {
    el.type.baseType = VarType::UByte;
    el.type.flags |= ShaderVariableFlags::UNorm;
  }
  else if(basetype == lit("snormh"))
  {
    el.type.baseType = VarType::SShort;
    el.type.flags |= ShaderVariableFlags::SNorm;
  }
  else if(basetype == lit("snormb"))
  {
    el.type.baseType = VarType::SByte;
    el.type.flags |= ShaderVariableFlags::SNorm;
  }
  else if(basetype == lit("uintten"))
  {
    el.type.baseType = VarType::UInt;
    el.type.flags |= ShaderVariableFlags::R10G10B10A2;
    el.type.columns = 4;
  }
  else if(basetype == lit("unormten"))
  {
    el.type.baseType = VarType::UInt;
    el.type.flags |= ShaderVariableFlags::R10G10B10A2;
    el.type.flags |= ShaderVariableFlags::UNorm;
    el.type.columns = 4;
  }
  else if(basetype == lit("floateleven"))
  {
    el.type.baseType = VarType::Float;
    el.type.flags |= ShaderVariableFlags::R11G11B10;
    el.type.columns = 3;
  }
  else
  {
    return false;
  }

  return true;
}

static QString MakeIdentifierName(const rdcstr &name)
{
  QString ret = name;

  ret = ret.replace(QLatin1Char('['), QLatin1Char('_')).replace(QLatin1Char(']'), QString());

  if(ret[0].isDigit())
    ret.prepend(QLatin1Char('_'));

  ret.replace(QRegularExpression(lit("[^A-Za-z0-9@_]+")), lit("_"));

  return ret;
}

void BufferFormatter::EstimatePackingRules(Packing::Rules &pack, ResourceId shader,
                                           const ShaderConstant &constant)
{
  // see if this constant violates any of the packing rules we are currently checking for.
  // We can't *prove* a rule is followed just from one example, we can only see if it is never
  // *disproved*. This does mean we won't necessarily determine the exact packing scheme, e.g if
  // scalar packing was used but it was only three float4 vectors then it will look like the most
  // conservative std140/scalar.

  if(!pack.vector_align_component || !pack.vector_straddle_16b)
  {
    // column major matrices have vectors that are 'rows' long. Everything else is vectors of
    // 'columns' long
    uint8_t vecSize = constant.type.columns;
    uint8_t matSize = constant.type.rows;

    if(constant.type.rows > 1 && constant.type.ColMajor())
    {
      vecSize = constant.type.rows;
      matSize = constant.type.columns;
    }

    if(vecSize > 1)
    {
      // is this a vector that's only component aligned and NOT vector aligned. If so,
      // vector_align_component is true
      const uint32_t vec4Size = VarTypeByteSize(constant.type.baseType) * 4;
      const uint32_t offsModVec = (constant.byteOffset % vec4Size);

      // if it's a vec3 or vec4 and its offset is not purely aligned, it's only component aligned
      if(vecSize >= 3 && offsModVec != 0)
        pack.vector_align_component = true;

      // if it's a vec2 and its offset is not either 0 or half the total size, it's also only
      // component aligned. vec2s without this allowance must be aligned to the vec2 size
      if(vecSize == 2 && offsModVec != 0 && offsModVec != vec4Size / 2)
        pack.vector_align_component = true;

      if(constant.type.elements > 1)
      {
        // with arrays we can check the stride as well. If the stride isn't vector-aligned then
        // that's the same as vectors being aligned to components (even if we don't see it)
        if(vecSize >= 3 && constant.type.arrayByteStride < vec4Size)
          pack.vector_align_component = true;
        if(vecSize == 2 && constant.type.arrayByteStride < vec4Size / 2)
          pack.vector_align_component = true;
      }

      if(matSize > 1)
      {
        if(vecSize >= 3 && constant.type.matrixByteStride < vec4Size)
          pack.vector_align_component = true;
        if(vecSize == 2 && constant.type.matrixByteStride < vec4Size / 2)
          pack.vector_align_component = true;
      }

      // while we're here, check if the vector straddles a 16-byte boundary

      const uint32_t low16b = (constant.byteOffset / 16);
      const uint32_t high16b =
          ((constant.byteOffset + VarTypeByteSize(constant.type.baseType) * vecSize - 1) / 16);

      // if the vector crosses a 16-byte boundary, vectors can straddle them
      if(low16b != high16b)
        pack.vector_straddle_16b = true;
    }

    if(!pack.tight_arrays && matSize > 1)
    {
      // if the array has a byte stride less than 16, it must be non-tight packed
      if(constant.type.matrixByteStride < 16)
        pack.tight_arrays = true;
    }
  }

  if(!pack.tight_arrays && constant.type.elements > 1)
  {
    // if the array has a byte stride less than 16, it must be non-tight packed
    if(constant.type.arrayByteStride < 16)
      pack.tight_arrays = true;
  }

  if(!pack.tight_arrays && constant.type.baseType == VarType::Struct)
  {
    // if a struct isn't padded to 16-byte alignment, assume non-tight arrays
    if((constant.type.arrayByteStride % 16) != 0)
      pack.tight_arrays = true;
  }

  EstimatePackingRules(pack, shader, constant.type.members);
}

void BufferFormatter::EstimatePackingRules(Packing::Rules &pack, ResourceId shader,
                                           const rdcarray<ShaderConstant> &members)
{
  for(size_t i = 0; i < members.size(); i++)
  {
    // check this constant
    EstimatePackingRules(pack, shader, members[i]);

    // when pointers are in use, follow the type and estimate with those too
    if(members[i].type.pointerTypeID != ~0U)
    {
      const ShaderConstantType &ptrType =
          PointerTypeRegistry::GetTypeDescriptor(shader, members[i].type.pointerTypeID);
      EstimatePackingRules(pack, shader, ptrType.members);
    }

    // check for trailing array/struct use
    if(i > 0)
    {
      Packing::Rules unpadded = pack;
      Packing::Rules padded = pack;
      unpadded.trailing_overlap = true;
      unpadded.vector_align_component = true;
      padded.trailing_overlap = false;

      const uint32_t unpaddedAdvance = GetVarAdvance(unpadded, members[i - 1]);
      const uint32_t paddedAdvance = GetVarAdvance(padded, members[i - 1]);

      // if we overlap into the previous element, and it contains padding, then trailing padding is
      // not reserved. This applies to structs, arrays and matrices.
      if(paddedAdvance > unpaddedAdvance &&
         members[i].byteOffset < (members[i - 1].byteOffset + paddedAdvance))
      {
        pack.trailing_overlap = true;
      }
    }

    // if we've degenerated to scalar we can't get any more lenient, stop checking rules
    if(pack == Packing::Scalar)
      break;
  }
}

Packing::Rules BufferFormatter::EstimatePackingRules(ResourceId shader,
                                                     const rdcarray<ShaderConstant> &members)
{
  Packing::Rules pack;

  // start from the most conservative ruleset. We will iteratively turn off any rules which are
  // violated to end up with the most conservative ruleset which is still valid for the described
  // variable

  // D3D shouldn't really need to be estimating, because it's implicit from how this is bound
  // (cbuffer or structured resource)
  if(IsD3D(m_API))
    pack = Packing::D3DCB;
  else
    pack = Packing::std140;

  EstimatePackingRules(pack, shader, members);

  // only return a 'real' ruleset. Don't revert to individually setting rules if we can help it
  // since that's a mess. The worst case is if someone is really using a custom packing format then
  // we add some extra offset decorations

  // only look for layouts typical of the API in use
  if(IsD3D(m_API))
  {
    // scalar is technically more lenient than anything D3D allows, as D3DUAV requires padding after
    // structs (it's closer to C packing)
    if(pack == Packing::D3DCB || pack == Packing::D3DUAV || pack == Packing::Scalar)
      return pack;

    // shouldn't end up with these as we started at D3DCB, but just for safety
    if(pack == Packing::std140)
      return Packing::D3DCB;

    if(pack == Packing::std430)
      return Packing::D3DUAV;
  }
  else
  {
    if(pack == Packing::std140 || pack == Packing::std430 || pack == Packing::Scalar)
      return pack;

    if(m_API == GraphicsAPI::Vulkan)
    {
      if(pack == Packing::D3DCB || pack == Packing::D3DUAV)
        return pack;

      // on vulkan HLSL shaders may use relaxed block layout, which is not wholly represented here.
      // it doesn't actually allow trailing overlap but this lets us check if we're 'almost' cbuffer
      // rules, at which point any instances where trailing overlap would be used will look just
      // like manual padding/offsetting
      Packing::Rules mod = pack;
      mod.trailing_overlap = true;

      if(mod == Packing::D3DCB)
        return Packing::D3DCB;
    }
  }

  // don't explicitly use C layout, revert to scalar which is more typical in graphics
  // the worst case is that some elements that would be in trailing padding in structs get explicit
  // offset annotations to move them out, since in C that would be implicit.
  //
  // note, D3DUAV is treated the same as C but we checked for it above so we'd only get here on
  // non-D3D
  if(pack == Packing::C)
    return Packing::Scalar;

  // our ruleset doesn't match exactly to a premade one. Check the rules to see which properties we
  // have.
  // Currently this always means devolving to scalar, but we lay it out explicitly like this in case
  // other rulesets are added in future.

  // only scalar layouts allow straddling 16 byte alignment, it would be very strange to allow
  // straddling 16 bytes but e.g. not have tight arrays or component-aligned vectors. Possibly no
  // arrays were seen so tight arrays couldn't be explicitly determined. So regardless of what else
  // we found return scalar
  if(pack.vector_straddle_16b)
    return Packing::Scalar;

  // trailing overlap is allowed in any D3D layout, but for non-D3D only in scalar layout.
  // Since we know from above that either we're not using D3D or we aren't an exact match for D3DCB,
  // assume we're in scalar one way or another.
  // This could be e.g. D3DUAV with tight arrays but vector straddling wasn't seen explicitly
  if(pack.trailing_overlap)
    return Packing::Scalar;

  // the exact same logic as above applies to component-aligned vectors. Allowed in any D3D layout,
  // but for non-D3D only in scalar layout.
  if(pack.vector_align_component)
    return Packing::Scalar;

  // For non-D3D: if we have tight arrays, this is possible in std430 - however since we didn't
  // match std430 above there must be some other allowance. That means we must devolve to scalar
  // For D3D this is possible only in D3DUAV (which is equivalent to scalar)
  if(pack.tight_arrays)
    return Packing::Scalar;

  // shouldn't get here, but just for safety return the ruleset we derived
  return pack;
}

QString BufferFormatter::DeclarePacking(Packing::Rules pack)
{
  if(pack == Packing::D3DCB)
    return lit("#pack(cbuffer)");
  else if(pack == Packing::std140)
    return lit("#pack(std140)");
  else if(pack == Packing::std430)
    return lit("#pack(std430)");
  else if(pack == Packing::D3DUAV)    // this is also C but we call it 'structured' for D3D
    return lit("#pack(structured)");
  else if(pack == Packing::Scalar)
    return lit("#pack(scalar)");

  // packing doesn't match a premade ruleset. Emit individual specifiers
  QString ret;
  if(pack.vector_align_component)
    ret += lit("#pack(vector_align_component)    // vectors are aligned to their component\n");
  else
    ret +=
        lit("#pack(no_vector_align_component) // vectors are aligned evenly (float3 as float4)\n");
  if(pack.tight_arrays)
    ret += lit("#pack(tight_arrays)              // arrays are packed tightly\n");
  else
    ret += lit("#pack(no_tight_arrays)           // arrays are padded to 16-byte boundaries\n");
  if(pack.vector_straddle_16b)
    ret += lit("#pack(vector_straddle_16b)       // vectors can straddle 16-byte boundaries\n");
  else
    ret += lit("#pack(no_vector_straddle_16b)    // vectors cannot straddle 16-byte boundaries\n");
  if(pack.trailing_overlap)
    ret +=
        lit("#pack(trailing_overlap)          // variables can overlap trailing padding after "
            "arrays/structs\n");
  else
    ret +=
        lit("#pack(no_trailing_overlap)       // variables cannot overlap trailing padding after "
            "arrays/structs\n");

  return ret.trimmed();
}

bool BufferFormatter::ContainsUnbounded(const ShaderConstant &structType,
                                        rdcpair<rdcstr, rdcstr> *found)
{
  const rdcarray<ShaderConstant> &members = structType.type.members;
  for(size_t i = 0; i < members.size(); i++)
  {
    if(members[i].type.elements == ~0U)
    {
      if(found)
        *found = {structType.name, members[i].name};
      return true;
    }

    if(ContainsUnbounded(members[i], found))
      return true;
  }

  return false;
}

bool BufferFormatter::CheckInvalidUnbounded(const StructFormatData &structData,
                                            const QMap<QString, StructFormatData> &structelems,
                                            QMap<int, QString> &errors)
{
  const ShaderConstant &def = structData.structDef;

  for(size_t i = 0; i < def.type.members.size(); i++)
  {
    const bool isLast = i == def.type.members.size() - 1;

    // if it's not the last member and it's unbounded, that's a problem!
    if(!isLast && def.type.members[i].type.elements == ~0U)
    {
      int line = structData.lineMemberDefs[(int)i];
      errors[line] = tr("Only the last member of a struct can be an unbounded array.");
      return false;
    }

    // if it's not the last member, no child can have an unbounded array
    rdcpair<rdcstr, rdcstr> unbounded;
    if(!isLast && ContainsUnbounded(def.type.members[i], &unbounded))
    {
      int line = structData.lineMemberDefs[(int)i];
      errors[line] =
          tr("Only the last member of a struct can contain an unbounded array. %1 in %2 is "
             "unbounded.")
              .arg(unbounded.second)
              .arg(unbounded.first);
      return false;
    }

    if(!CheckInvalidUnbounded(structelems[def.type.members[i].type.name], structelems, errors))
      return false;
  }

  return true;
}

ParsedFormat BufferFormatter::ParseFormatString(const QString &formatString, uint64_t maxLen,
                                                bool cbuffer)
{
  ParsedFormat ret;

  StructFormatData root;
  StructFormatData *cur = &root;

  QMap<QString, StructFormatData> structelems;
  QString lastStruct;

  // regex doesn't account for trailing or preceeding whitespace, or comments

  QRegularExpression regExpr(
      lit("^"                                            // start of the line
          "(?<major>row_major\\s+|column_major\\s+)?"    // matrix majorness
          "(?<sign>unsigned\\s+|signed\\s+)?"            // allow 'signed int' or 'unsigned char'
          "(?<rgb>rgb\\s+)?"                             // rgb element colourising
          "(?<type>"                                     // group the options for the type
          "uintten|unormten"                             // R10G10B10A2 types
          "|floateleven"                                 // R11G11B10 special type
          "|unormh|unormb"                               // UNORM 16-bit and 8-bit types
          "|snormh|snormb"                               // SNORM 16-bit and 8-bit types
          "|bool"                                        // bool is stored as 4-byte int
          "|byte|short|int|long|char"                    // signed ints
          "|ubyte|ushort|uint|ulong"                     // unsigned ints
          "|xbyte|xshort|xint|xlong"                     // hex ints
          "|half|float|double"                           // float types
          "|vec|uvec|ivec|dvec"                          // OpenGL vector types
          "|mat|umat|imat|dmat"                          // OpenGL matrix types
          "|int8_t|uint8_t"                              // C-style sized 8-bit types
          "|int16_t|uint16_t"                            // C-style sized 16-bit types
          "|int32_t|uint32_t"                            // C-style sized 32-bit types
          "|int64_t|uint64_t"                            // C-style sized 64-bit types
          "|float16_t|float32_t|float64_t"               // C-style sized float types
          ")"                                            // end of the type group
          "(?<vec>[1-9])?"                               // might be a vector
          "(?<mat>x[1-9])?"                              // or a matrix
          "(?<name>\\s+[A-Za-z@_][A-Za-z0-9@_]*)?"       // get identifier name
          "(?<array>\\s*\\[[0-9]*\\])?"                  // optional array dimension
          "(\\s*:\\s*"                                   // optional specifier after :
          "("                                            // bitfield or semantic
          "(?<bitfield>[1-9][0-9]*)|"                    // bitfield packing
          "(?<semantic>[A-Za-z_][A-Za-z0-9_]*)"          // semantic to ignore
          ")"                                            // end bitfield or semantic
          ")?"
          "$"));

  bool success = true;

  // remove any dos newlines
  QString text = formatString;
  text.replace(lit("\r\n"), lit("\n"));

  QRegularExpression annotationRegex(
      lit("^"                           // start of the line
          "\\[\\["                      // opening [[
          "(?<name>[a-zA-Z0-9_-]+)"     // annotation name
          "(\\((?<param>[^)]+)\\))?"    // optional parameter in ()s
          "\\]\\]"                      // closing ]]
          "\\s*"));

  QRegularExpression structDeclRegex(
      lit("^(struct|enum)\\s+([A-Za-z_][A-Za-z0-9_]*)(\\s*:\\s*([a-z]+))?$"));
  QRegularExpression structUseRegex(
      lit("^"                              // start of the line
          "([A-Za-z_][A-Za-z0-9_]*)"       // struct type name
          "([ \\t\\r\\n*]+)"               // maybe a pointer, but at least some whitespace
          "([A-Za-z@_][A-Za-z0-9@_]*)?"    // variable name
          "(\\s*\\[[0-9]*\\])?"            // optional array dimension
          "(\\s*:\\s*([1-9][0-9]*))?"      // optional bitfield packing
          "$"));
  QRegularExpression enumValueRegex(
      lit("^"                              // start of the line
          "([A-Za-z_][A-Za-z0-9_]*)"       // value name
          "\\s*=\\s*"                      // maybe a pointer
          "(-?0x[0-9a-fA-F]+|-?[0-9]+)"    // numerical value
          "$"));

  QRegularExpression bitfieldSkipRegex(
      lit("^"                             // start of the line
          "(unsigned\\s+|signed\\s+)?"    // allow 'signed int' or 'unsigned char'
          "("                             // type group
          "|bool"                         // bool is stored as 4-byte int
          "|byte|short|int|long|char"     // signed ints
          "|ubyte|ushort|uint|ulong"      // unsigned ints
          "|xbyte|xshort|xint|xlong"      // hex ints
          ")"                             // end of the type group
                                          // no variable name
          "\\s*:\\s*([1-9][0-9]*)"        // bitfield packing
          "$"));

  QRegularExpression packingRegex(
      lit("^"                         // start of the line
          "#\\s*pack\\s*\\("          // #pack(
          "(?<rule>[a-zA-Z0-9_]+)"    // packing ruleset or individual rule
          "\\)"                       // )
          "$"));

  uint32_t bitfieldCurPos = ~0U;

  struct Annotation
  {
    QString name;
    QString param;
  };

  // default to scalar (tight packing) if nothing else is specified at all. The expectation is
  // anything that needs a better default will insert that into the format string for the user,
  // or be picked up below
  Packing::Rules &pack = ret.packing;
  pack = Packing::Scalar;

  // for D3D and GL we default to the only valid packing for cbuffers and UAVs. The user can still
  // override this if they really wish with a #pack, but this makes sense as a sensible default
  if(cbuffer)
  {
    if(IsD3D(m_API))
      pack = Packing::D3DCB;
    else if(m_API == GraphicsAPI::OpenGL)
      pack = Packing::std140;
  }
  else
  {
    if(IsD3D(m_API))
      pack = Packing::D3DUAV;
    else if(m_API == GraphicsAPI::OpenGL)
      pack = Packing::std430;
  }
  // vulkan allows scalar packing in any buffer, so don't wrest control away from the user

  int line = 0;

  QMap<int, QString> &errors = ret.errors;
  auto reportError = [&line, &errors](QString err) { errors[line] = err.trimmed(); };

  QList<Annotation> annotations;

  QString parseText = text;
  int parseLine = 0;

  // get each line and parse it to determine the format the user wanted
  while(!parseText.isEmpty())
  {
    // consume up to the next terminator (comma, semicolon, brace, or newline) while ignore C and
    // C++ style comments, as well as counting newlines for line numbers

    enum parsestate
    {
      NORMAL,
      C_COMMENT,
      CPP_COMMENT
    } state = NORMAL;

    QString decl;

    {
      int end = 0;
      for(; end < parseText.length();)
      {
        // peek ahead character
        QChar c = parseText[end];

        // if we have a non-empty declaration and we're about to hit a brace, stop now before
        // actually processing it.
        const bool brace = (c == QLatin1Char('{') || c == QLatin1Char('}'));
        if(brace && !decl.trimmed().isEmpty())
          break;

        // consume c now, whatever it is, we've read it and will process it below
        end++;

        // if this is a ; or , we don't bother to include it in the declaration but stop now
        if(state == NORMAL && (c == QLatin1Char(';') || c == QLatin1Char(',')))
          break;

        if(c == QLatin1Char('\n'))
        {
          parseLine++;

          // if we're in a CPP comment, go back to normal
          if(state == CPP_COMMENT)
            state = NORMAL;

          // if we have a preprocessor definition (first non-whitespace character is #) end at the
          // end of the line without a ;
          if(state == NORMAL && decl.trimmed().startsWith(lit("#")))
            break;
        }

        QChar c2;
        if(end + 1 < parseText.length())
          c2 = parseText[end];

        if(state == NORMAL && c == QLatin1Char('/') && c2 == QLatin1Char('/'))
        {
          // consume the next character too
          end++;
          state = CPP_COMMENT;
          continue;
        }

        if(state == NORMAL && c == QLatin1Char('/') && c2 == QLatin1Char('*'))
        {
          // consume the next character too
          end++;
          state = C_COMMENT;
          continue;
        }

        if(state == C_COMMENT && c == QLatin1Char('*') && c2 == QLatin1Char('/'))
        {
          // consume the next character too
          end++;
          state = NORMAL;
          continue;
        }

        if(state == NORMAL)
        {
          decl.append(c);
          line = parseLine;

          // braces should be considered their own declarations
          if(brace)
            break;
        }
      }

      parseText = parseText.mid(end);
      decl = decl.trimmed();
    }

    if(decl.isEmpty())
      continue;

    do
    {
      QRegularExpressionMatch match = annotationRegex.match(decl);

      if(!match.hasMatch())
        break;

      annotations.push_back({match.captured(lit("name")), match.captured(lit("param"))});

      decl.remove(match.capturedStart(0), match.capturedLength(0));
      decl = decl.trimmed();
    } while(true);

    if(decl.isEmpty())
      continue;

    if(decl[0] == QLatin1Char('#'))
    {
      QRegularExpressionMatch match = packingRegex.match(decl);

      if(match.hasMatch())
      {
        if(cur != &root)
        {
          reportError(tr("Packing rules can only be changed at global scope."));
          success = false;
          break;
        }

        QString packrule = match.captured(lit("rule")).toLower();

        // try to pick up common aliases that people might use
        if(packrule == lit("d3dcbuffer") || packrule == lit("cbuffer") || packrule == lit("cb"))
          pack = Packing::D3DCB;
        else if(packrule == lit("d3duav") || packrule == lit("uav") || packrule == lit("structured"))
          pack = Packing::D3DUAV;
        else if(packrule == lit("std140") || packrule == lit("ubo") || packrule == lit("gl") ||
                packrule == lit("gles") || packrule == lit("opengl") || packrule == lit("glsl"))
          pack = Packing::std140;
        else if(packrule == lit("std430") || packrule == lit("ssbo"))
          pack = Packing::std430;
        else if(packrule == lit("scalar"))
          pack = Packing::Scalar;
        else if(packrule == lit("c"))
          pack = Packing::C;

        // we also allow toggling the individual rules
        else if(packrule == lit("vector_align_component"))
          pack.vector_align_component = true;
        else if(packrule == lit("no_vector_align_component"))
          pack.vector_align_component = false;
        else if(packrule == lit("tight_arrays"))
          pack.tight_arrays = true;
        else if(packrule == lit("no_tight_arrays"))
          pack.tight_arrays = false;
        else if(packrule == lit("vector_straddle_16b"))
          pack.vector_straddle_16b = true;
        else if(packrule == lit("no_vector_straddle_16b"))
          pack.vector_straddle_16b = false;
        else if(packrule == lit("trailing_overlap"))
          pack.trailing_overlap = true;
        else if(packrule == lit("no_trailing_overlap"))
          pack.trailing_overlap = false;

        else if(packrule == lit("tight_bitfield_packing"))
          pack.tight_bitfield_packing = true;
        else if(packrule == lit("no_tight_bitfield_packing"))
          pack.tight_bitfield_packing = false;

        else
          packrule = QString();

        if(packrule.isEmpty())
        {
          reportError(tr("Unrecognised packing rule specifier '%1'.\n\n"
                         "Supported rulesets:\n"
                         " - cbuffer (D3D constant buffer packing)\n"
                         " - uav (D3D UAV packing)\n"
                         " - std140 (GL/Vulkan std140 packing)\n"
                         " - std430 (GL/Vulkan std430 packing)\n"
                         " - scalar (Tight scalar packing)")
                          .arg(packrule));
          success = false;
          break;
        }

        continue;
      }
      else
      {
        reportError(tr("Unrecognised pre-processor command '%1'.\n\n"
                       "Pre-processor commands must be all on one line.\n")
                        .arg(decl));
        success = false;
        break;
      }
    }

    if(cur == &root)
    {
      // if we're not in a struct, ignore the braces
      if(decl == lit("{") || decl == lit("}"))
        continue;
    }
    else
    {
      // if we're in a struct, ignore the opening brace and revert back to root elements when we hit
      // the closing brace. No brace nesting is supported
      if(decl == lit("{"))
        continue;

      if(decl == lit("}"))
      {
        if(bitfieldCurPos != ~0U)
        {
          // update final offset to account for any bits consumed by a trailing bitfield, including
          // any bits in the last byte that weren't allocated
          cur->offset += (bitfieldCurPos + 7) / 8;

          // reset bitpacking state.
          bitfieldCurPos = ~0U;
        }

        if(cur->structDef.type.baseType == VarType::Struct)
        {
          cur->structDef.type.arrayByteStride = cur->offset;

          cur->alignment = GetAlignment(pack, cur->structDef);

          // if we don't have tight arrays, struct byte strides are always 16-byte aligned
          if(!pack.tight_arrays)
          {
            cur->alignment = 16;
          }

          cur->structDef.type.arrayByteStride = AlignUp(cur->offset, cur->alignment);

          if(cur->paddedStride > 0)
          {
            // only pad up to the stride, not down
            if(cur->paddedStride >= cur->structDef.type.arrayByteStride)
            {
              cur->structDef.type.arrayByteStride = cur->paddedStride;
            }
            else
            {
              reportError(tr("Struct %1 declared size %2 bytes is less than derived structure "
                             "size %3 bytes.")
                              .arg(cur->structDef.type.name)
                              .arg(cur->paddedStride)
                              .arg(cur->structDef.type.arrayByteStride));
              success = false;
              break;
            }
          }

          cur->pointerTypeId = PointerTypeRegistry::GetTypeID(cur->structDef.type);
        }

        cur = &root;
        continue;
      }
    }

    if(decl.startsWith(lit("struct")) || decl.startsWith(lit("enum")))
    {
      QRegularExpressionMatch match = structDeclRegex.match(decl);

      if(match.hasMatch())
      {
        QString typeName = match.captured(1);
        QString name = match.captured(2);

        if(structelems.contains(name))
        {
          reportError(tr("type %1 has already been defined.").arg(name));
          success = false;
          break;
        }

        cur = &structelems[name];
        cur->structDef.type.name = name;
        bitfieldCurPos = ~0U;

        if(typeName == lit("struct"))
        {
          lastStruct = name;
          cur->structDef.type.baseType = VarType::Struct;

          for(const Annotation &annot : annotations)
          {
            if(annot.name == lit("size") || annot.name == lit("byte_size"))
            {
              if(annot.param.isEmpty())
              {
                reportError(tr("Annotation '%1' requires a parameter with the size in bytes.\n\n"
                               "e.g. [[%1(128)]]")
                                .arg(annot.name));
                success = false;
                break;
              }
              cur->paddedStride = annot.param.toUInt();
            }
            else if(annot.name == lit("single") || annot.name == lit("fixed"))
            {
              cur->singleDef = true;
            }
            else
            {
              reportError(
                  tr("Unrecognised annotation '%1' on struct definition.\n\n"
                     "Supported struct annotations:\n"
                     " - [[size(x)]] specify the size to pad the struct to.\n"
                     " - [[single]] specify that this struct is fixed, not array-of-structs.")
                      .arg(annot.name));
              success = false;
              break;
            }
          }

          annotations.clear();

          if(!success)
            break;
        }
        else
        {
          cur->structDef.type.baseType = VarType::Enum;

          for(const Annotation &annot : annotations)
          {
            if(false)
            {
              // no annotations supported currently on enums
            }
            else
            {
              reportError(tr("Unrecognised annotation '%1' on enum definition.").arg(annot.name));
              success = false;
              break;
            }
          }

          annotations.clear();

          if(!success)
            break;

          QString baseType = match.captured(4);

          if(baseType.isEmpty())
          {
            reportError(
                tr("Enum declarations require a sized base type. E.g. enum %1 : uint").arg(name));
            success = false;
            break;
          }

          ShaderConstant tmp;

          bool matched = MatchBaseTypeDeclaration(baseType, false, tmp);

          if(!matched ||
             (VarTypeCompType(tmp.type.baseType) != CompType::UInt &&
              VarTypeCompType(tmp.type.baseType) != CompType::SInt) ||
             tmp.type.flags != ShaderVariableFlags::NoFlags)
          {
            reportError(tr("Invalid enum base type '%1', must be an integer type.").arg(baseType));
            success = false;
            break;
          }

          cur->structDef.type.matrixByteStride = VarTypeByteSize(tmp.type.baseType);
          cur->signedEnum = (VarTypeCompType(tmp.type.baseType) == CompType::SInt);
        }

        continue;
      }
    }

    ShaderConstant el;

    if(cur->structDef.type.baseType == VarType::Enum)
    {
      QRegularExpressionMatch enumMatch = enumValueRegex.match(decl);

      if(!enumMatch.hasMatch())
      {
        reportError(tr("Couldn't parse value declaration in enum."));
        success = false;
        break;
      }

      QString valueNum = enumMatch.captured(2);

      bool ok = false;
      if(cur->signedEnum)
      {
        int64_t val = valueNum.toLongLong(&ok, 0);

        if(ok)
        {
          // convert signed 'literally' to unsigned and truncate
          if(cur->structDef.type.matrixByteStride == 1)
          {
            if(val > INT8_MAX || val < INT8_MIN)
            {
              reportError(
                  tr("Enum with 8-bit signed integer type cannot hold value '%1'.").arg(valueNum));
              success = false;
              break;
            }

            int8_t truncVal = (int8_t)val;
            memcpy(&el.defaultValue, &truncVal, sizeof(truncVal));
          }
          else if(cur->structDef.type.matrixByteStride == 2)
          {
            if(val > INT16_MAX || val < INT16_MIN)
            {
              reportError(
                  tr("Enum with 16-bit signed integer type cannot hold value '%1'.").arg(valueNum));
              success = false;
              break;
            }

            int16_t truncVal = (int16_t)val;
            memcpy(&el.defaultValue, &truncVal, sizeof(truncVal));
          }
          else if(cur->structDef.type.matrixByteStride == 4)
          {
            if(val > INT32_MAX || val < INT32_MIN)
            {
              reportError(
                  tr("Enum with 32-bit signed integer type cannot hold value '%1'.").arg(valueNum));
              success = false;
              break;
            }

            int32_t truncVal = (int32_t)val;
            memcpy(&el.defaultValue, &truncVal, sizeof(truncVal));
          }
          else if(cur->structDef.type.matrixByteStride == 8)
          {
            el.defaultValue = valueNum.toULongLong();
          }
        }

        if(!ok)
        {
          reportError(tr("Couldn't parse enum numerical value from '%1'.").arg(valueNum));
          success = false;
          break;
        }
      }
      else
      {
        if(valueNum[0] == QLatin1Char('-'))
        {
          reportError(tr("Enum with unsigned base type cannot have signed value."));
          success = false;
          break;
        }

        el.defaultValue = valueNum.toULongLong(&ok, 0);

        if(el.defaultValue > (UINT64_MAX >> (64 - 8 * cur->structDef.type.matrixByteStride)))
        {
          reportError(tr("Enum with %1-bit signed integer type cannot hold value '%1'.")
                          .arg(8 * cur->structDef.type.matrixByteStride)
                          .arg(valueNum));
          success = false;
          break;
        }

        if(!ok)
        {
          valueNum.toULongLong(&ok, 0);
          reportError(tr("Couldn't get enum numerical value from '%1'.").arg(valueNum));
          success = false;
          break;
        }
      }

      el.name = enumMatch.captured(1);

      for(const Annotation &annot : annotations)
      {
        if(false)
        {
          // no annotations supported currently on enums
        }
        else
        {
          reportError(tr("Unrecognised annotation '%1' on enum value.").arg(annot.name));
          success = false;
          break;
        }
      }

      annotations.clear();

      if(!success)
        break;

      cur->structDef.type.members.push_back(el);
      cur->lineMemberDefs.push_back(line);

      continue;
    }

    QRegularExpressionMatch bitfieldSkipMatch = bitfieldSkipRegex.match(decl);

    if(bitfieldSkipMatch.hasMatch())
    {
      if(bitfieldCurPos == ~0U)
        bitfieldCurPos = 0;
      bitfieldCurPos += bitfieldSkipMatch.captured(3).toUInt();

      for(const Annotation &annot : annotations)
      {
        if(false)
        {
          // no annotations supported currently on enums
        }
        else
        {
          reportError(tr("Unrecognised annotation '%1' on bitfield skip element.").arg(annot.name));
          success = false;
          break;
        }
      }

      annotations.clear();

      if(!success)
        break;

      continue;
    }

    if(cur->singleMember)
    {
      reportError(
          tr("[[single]] can only be used if there is only one variable in the root.\n"
             "Consider wrapping the variables in a struct and annotating it as [[single]]."));
      success = false;
      break;
    }

    QRegularExpressionMatch structMatch = structUseRegex.match(decl);

    bool isPadding = false;

    if(structMatch.hasMatch() && structelems.contains(structMatch.captured(1)))
    {
      StructFormatData &structContext = structelems[structMatch.captured(1)];

      QString pointerStars = structMatch.captured(2).trimmed();
      bool isPointer = !pointerStars.isEmpty();

      if(pointerStars.count() > 1)
      {
        reportError(tr("Only single pointers are supported."));
        success = false;
        break;
      }

      if(structContext.singleDef)
      {
        reportError(tr("[[single]] annotated structs can't be used, only defined."));
        success = false;
        break;
      }

      if(!isPointer && structContext.structDef.type.name == cur->structDef.type.name)
      {
        reportError(tr("Invalid nested struct declaration, only allowed for pointers."));
        success = false;
        break;
      }

      QString varName = structMatch.captured(3).trimmed();

      if(varName.isEmpty())
        varName = lit("data");

      uint32_t specifiedOffset = ~0U;
      for(const Annotation &annot : annotations)
      {
        if(annot.name == lit("offset") || annot.name == lit("byte_offset"))
        {
          if(annot.param.isEmpty())
          {
            reportError(tr("Annotation '%1' requires a parameter with the offset in bytes.\n\n"
                           "e.g. [[%1(128)]]")
                            .arg(annot.name));
            success = false;
            break;
          }
          specifiedOffset = annot.param.toUInt();
        }
        else if(annot.name == lit("pad") || annot.name == lit("padding"))
        {
          isPadding = true;
        }
        else if(annot.name == lit("single") || annot.name == lit("fixed"))
        {
          if(cur != &root)
          {
            reportError(tr("[[single]] can only be used on global variables."));
            success = false;
            break;
          }
          else if(!cur->structDef.type.members.empty())
          {
            reportError(
                tr("[[single]] can only be used if there is only one variable in the root.\n"
                   "Consider wrapping the variables in a struct and marking it as [[single]]."));
            success = false;
            break;
          }
          else
          {
            cur->singleMember = true;
          }
        }
        else
        {
          reportError(tr("Unrecognised annotation '%1' on variable.").arg(annot.name));
          success = false;
          break;
        }
      }

      if(!success)
        break;

      annotations.clear();

      QString arrayDim = structMatch.captured(4).trimmed();
      uint32_t arrayCount = 1;
      if(!arrayDim.isEmpty())
      {
        arrayDim = arrayDim.mid(1, arrayDim.count() - 2);
        if(arrayDim.isEmpty())
          arrayDim = lit("%1").arg(~0U);
        bool ok = false;
        arrayCount = arrayDim.toUInt(&ok);
        if(!ok)
          arrayCount = 1;
      }

      if(cur->singleMember && arrayCount == ~0U)
      {
        reportError(tr("[[single]] can't be used on unbounded arrays."));
        success = false;
        break;
      }

      QString bitfield = structMatch.captured(6).trimmed();

      if(isPointer)
      {
        if(!bitfield.isEmpty())
        {
          reportError(tr("Pointers can't be packed into a bitfield."));
          success = false;
          break;
        }

        // align to scalar size
        cur->offset = AlignUp(cur->offset, 8U);

        if(specifiedOffset != ~0U)
        {
          if(specifiedOffset < cur->offset)
          {
            reportError(tr("Specified byte offset %1 overlaps with previous data.\n"
                           "This value must be at byte offset %2 at minimum.")
                            .arg(specifiedOffset)
                            .arg(cur->offset));
            success = false;
            break;
          }

          cur->offset = specifiedOffset;
        }

        el.name = varName;
        el.byteOffset = cur->offset;
        el.type.pointerTypeID = structContext.pointerTypeId;
        el.type.baseType = VarType::GPUPointer;
        el.type.flags |= ShaderVariableFlags::HexDisplay;
        el.type.arrayByteStride = 8;
        el.type.elements = arrayCount;

        cur->offset += 8 * arrayCount;

        if(!isPadding)
        {
          cur->structDef.type.members.push_back(el);
          cur->lineMemberDefs.push_back(line);
        }

        continue;
      }
      else if(structContext.structDef.type.baseType == VarType::Enum)
      {
        if(!bitfield.isEmpty() && !arrayDim.isEmpty())
        {
          reportError(tr("Arrays can't be packed into a bitfield."));
          success = false;
          break;
        }

        // align to scalar size (if not bit packing)
        if(bitfieldCurPos == ~0U)
          cur->offset = AlignUp(cur->offset, (uint32_t)structContext.structDef.type.matrixByteStride);

        if(specifiedOffset != ~0U)
        {
          uint32_t offs = cur->offset;
          if(bitfieldCurPos != ~0U)
            offs += (bitfieldCurPos + 7) / 8;

          if(specifiedOffset < offs)
          {
            reportError(tr("Specified byte offset %1 overlaps with previous data.\n"
                           "This value must be at byte offset %2 at minimum.")
                            .arg(specifiedOffset)
                            .arg(offs));
            success = false;
            break;
          }

          cur->offset = specifiedOffset;

          // reset any bitfield packing to start at 0 at the new location
          if(bitfieldCurPos != ~0U)
            bitfieldCurPos = 0;
        }

        el = structContext.structDef;
        el.name = varName;
        el.byteOffset = cur->offset;
        el.type.elements = arrayCount;

        bool ok = false;
        el.bitFieldSize = qMax(1U, bitfield.toUInt(&ok));
        if(!ok)
          el.bitFieldSize = 0;

        // don't continue here - we will go through and handle bitfield packing like any other
        // scalar
      }
      else
      {
        if(!bitfield.isEmpty())
        {
          reportError(tr("Struct variables can't be packed into a bitfield."));
          success = false;
          break;
        }

        // all packing rules align structs in the same way as arrays. We already calculated this
        // when calculating the struct's alignment which will be padded to 16B for non-tight arrays
        cur->offset = AlignUp(cur->offset, structContext.alignment);

        if(specifiedOffset != ~0U)
        {
          if(specifiedOffset < cur->offset)
          {
            reportError(tr("Specified byte offset %1 overlaps with previous data.\n"
                           "This value must be at byte offset %2 at minimum.")
                            .arg(specifiedOffset)
                            .arg(cur->offset));
            success = false;
            break;
          }

          cur->offset = specifiedOffset;
        }

        el = structContext.structDef;
        el.name = varName;
        el.byteOffset = cur->offset;
        el.type.elements = arrayCount;

        if(!isPadding)
        {
          cur->structDef.type.members.push_back(el);
          cur->lineMemberDefs.push_back(line);
        }

        // advance by the struct including any trailing padding
        cur->offset += el.type.elements * el.type.arrayByteStride;

        // if we allow trailing overlap, remove the padding
        if(pack.trailing_overlap)
          cur->offset -= el.type.arrayByteStride - structContext.offset;

        continue;
      }
    }
    else
    {
      QRegularExpressionMatch match = regExpr.match(decl);

      if(!match.hasMatch())
      {
        QString problemGuess;

        // try to guess the problem since we don't have a proper parser and are just using regex's,
        // so we don't have a parse state to mention
        ShaderConstant dummy;
        int numRecognisedTypes = 0;
        QStringList identifiers = decl.split(QRegularExpression(lit("\\s+")));
        for(const QString &identifier : identifiers)
        {
          bool known = MatchBaseTypeDeclaration(identifier, false, dummy);
          if(known)
            numRecognisedTypes++;
        }

        // if we recognised more than one type maybe this is multiple lines that got combined
        if(numRecognisedTypes > 1)
        {
          problemGuess = tr("Did you need a ; between multiple declarations?");
        }
        else if(identifiers.size() >= 1 && structelems.contains(identifiers[0]))
        {
          problemGuess = tr("Invalid declaration of struct '%1'.").arg(identifiers[0]);
        }
        else if(identifiers.size() > 1)
        {
          problemGuess = tr("Unrecognised type '%1'.").arg(identifiers[0]);
        }

        reportError(
            tr("Failed to parse declaration:\n\n%1\n\n%2").arg(decl).arg(problemGuess).trimmed());
        success = false;
        break;
      }

      el.name = !match.captured(lit("name")).isEmpty() ? match.captured(lit("name")).trimmed()
                                                       : lit("data");

      QString basetype = match.captured(lit("type"));
      if(match.captured(lit("major")).trimmed() == lit("row_major"))
        el.type.flags |= ShaderVariableFlags::RowMajorMatrix;
      if(!match.captured(lit("rgb")).isEmpty())
        el.type.flags |= ShaderVariableFlags::RGBDisplay;
      QString firstDim =
          !match.captured(lit("vec")).isEmpty() ? match.captured(lit("vec")) : lit("1");
      QString secondDim =
          !match.captured(lit("mat")).isEmpty() ? match.captured(lit("mat")).mid(1) : lit("1");
      QString arrayDim = !match.captured(lit("array")).isEmpty()
                             ? match.captured(lit("array")).trimmed()
                             : lit("[1]");

      {
        bool isArray = !arrayDim.isEmpty();
        arrayDim = arrayDim.mid(1, arrayDim.count() - 2).trimmed();
        if(isArray && arrayDim.isEmpty())
          arrayDim = lit("%1").arg(~0U);
      }

      const bool isUnsigned = match.captured(lit("sign")).trimmed() == lit("unsigned");

      QString bitfield = match.captured(lit("bitfield"));

      QString vecMatSizeSuffix;

      // if we have a matrix and it's not GL style, then typeAxB means A rows and B columns
      // for GL matAxB that means A columns and B rows. This is in contrast to typeA which means A
      // columns for HLSL and A columns for GLSL, hence only the swap for matrices
      if(!match.captured(lit("mat")).isEmpty() && basetype != lit("mat"))
      {
        vecMatSizeSuffix = match.captured(lit("vec")) + match.captured(lit("mat"));
        firstDim.swap(secondDim);
      }
      else
      {
        if(!match.captured(lit("mat")).isEmpty())
          vecMatSizeSuffix = match.captured(lit("mat")).mid(1) + lit("x");
        vecMatSizeSuffix += match.captured(lit("vec"));
      }

      // check for square matrix declarations like 'mat4' and 'mat3'
      if(basetype == lit("mat") && match.captured(lit("mat")).isEmpty())
      {
        secondDim = firstDim;
        vecMatSizeSuffix = firstDim + lit("x") + firstDim;
      }

      // check for square matrix declarations like 'mat4' and 'mat3'
      if(basetype == lit("mat") && match.captured(lit("mat")).isEmpty())
        secondDim = firstDim;

      // calculate format
      {
        bool ok = false;

        el.type.columns = firstDim.toUInt(&ok);
        if(!ok)
        {
          reportError(tr("Invalid vector dimension '%1'.").arg(firstDim));
          success = false;
          break;
        }

        el.type.elements = qMax(1U, arrayDim.toUInt(&ok));
        if(!ok)
          el.type.elements = 1;

        if(!bitfield.isEmpty() && el.type.elements > 1)
        {
          reportError(tr("Arrays can't be packed into a bitfield."));
          success = false;
          break;
        }

        el.type.rows = qMax(1U, secondDim.toUInt(&ok));
        if(!ok)
        {
          reportError(tr("Invalid matrix dimension '%1'.").arg(secondDim));
          success = false;
          break;
        }

        el.bitFieldSize = qMax(1U, bitfield.toUInt(&ok));
        if(!ok)
          el.bitFieldSize = 0;

        // vectors are marked as row-major by convention
        if(el.type.rows == 1)
          el.type.flags |= ShaderVariableFlags::RowMajorMatrix;

        bool matched = MatchBaseTypeDeclaration(basetype, isUnsigned, el);

        if(!matched)
        {
          reportError(tr("Unrecognised type '%1'.").arg(basetype));
          success = false;
          break;
        }
      }

      el.type.name = ToStr(el.type.baseType) + vecMatSizeSuffix;

      // process packing annotations first, so we have that information to validate e.g. [[unorm]]
      for(const Annotation &annot : annotations)
      {
        if(annot.name == lit("packed"))
        {
          if(annot.param.toLower() == lit("r11g11b10"))
          {
            if(el.type.columns != 3 || el.type.baseType != VarType::Float)
            {
              reportError(tr("R11G11B10 packing must be specified on a 'float3' variable."));
              success = false;
              break;
            }

            el.type.flags |= ShaderVariableFlags::R11G11B10;
          }
          else if(annot.param.toLower() == lit("r10g10b10a2") ||
                  annot.param.toLower() == lit("r10g10b10a2_uint"))
          {
            if(el.type.columns != 4 || el.type.baseType != VarType::UInt)
            {
              reportError(
                  tr("R10G10B10A2 packing must be specified on a 'uint4' variable "
                     "(optionally with [[unorm]] or [[snorm]])."));
              success = false;
              break;
            }

            el.type.flags |= ShaderVariableFlags::R10G10B10A2;
          }
          else if(annot.param.toLower() == lit("r10g10b10a2_unorm"))
          {
            if(el.type.columns != 4 || el.type.baseType != VarType::UInt)
            {
              reportError(tr("R10G10B10A2_UNORM packing must be specified on a 'uint4' variable."));
              success = false;
              break;
            }

            el.type.flags |= ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::UNorm;
          }
          else if(annot.param.toLower() == lit("r10g10b10a2_snorm"))
          {
            if(el.type.columns != 4 || el.type.baseType != VarType::SInt)
            {
              reportError(tr("R10G10B10A2_SNORM packing must be specified on a 'int4' variable."));
              success = false;
              break;
            }

            el.type.flags |= ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::SNorm;
          }
          else if(annot.param.isEmpty())
          {
            reportError(tr("Annotation '%1' requires a parameter with the format packing.\n\n"
                           "e.g. [[%1(r10g10b10a2)]]")
                            .arg(annot.name));
            success = false;
            break;
          }
          else
          {
            reportError(tr("Unrecognised format packing '%1'.\n").arg(annot.param));
            success = false;
            break;
          }
        }
      }

      if(!success)
        break;

      for(const Annotation &annot : annotations)
      {
        if(annot.name == lit("rgb"))
        {
          el.type.flags |= ShaderVariableFlags::RGBDisplay;
        }
        else if(annot.name == lit("hex") || annot.name == lit("hexadecimal"))
        {
          if(VarTypeCompType(el.type.baseType) == CompType::Float)
          {
            reportError(tr("Hex display is not supported on floating point variables."));
            success = false;
            break;
          }

          if(el.type.flags & (ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::R11G11B10))
          {
            reportError(tr("Hex display is not supported on packed formats."));
            success = false;
            break;
          }

          el.type.flags |= ShaderVariableFlags::HexDisplay;

          if(el.type.baseType == VarType::SLong)
            el.type.baseType = VarType::ULong;
          else if(el.type.baseType == VarType::SInt)
            el.type.baseType = VarType::UInt;
          else if(el.type.baseType == VarType::SShort)
            el.type.baseType = VarType::UShort;
          else if(el.type.baseType == VarType::SByte)
            el.type.baseType = VarType::UByte;
        }
        else if(annot.name == lit("bin") || annot.name == lit("binary"))
        {
          if(VarTypeCompType(el.type.baseType) == CompType::Float)
          {
            reportError(tr("Binary display is not supported on floating point variables."));
            success = false;
            break;
          }

          if(el.type.flags & (ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::R11G11B10))
          {
            reportError(tr("Binary display is not supported on packed formats."));
            success = false;
            break;
          }

          el.type.flags |= ShaderVariableFlags::BinaryDisplay;

          if(el.type.baseType == VarType::SLong)
            el.type.baseType = VarType::ULong;
          else if(el.type.baseType == VarType::SInt)
            el.type.baseType = VarType::UInt;
          else if(el.type.baseType == VarType::SShort)
            el.type.baseType = VarType::UShort;
          else if(el.type.baseType == VarType::SByte)
            el.type.baseType = VarType::UByte;
        }
        else if(annot.name == lit("unorm"))
        {
          if(!(el.type.flags & ShaderVariableFlags::R10G10B10A2))
          {
            // verify that we're integer typed and 1 or 2 bytes
            if(el.type.baseType != VarType::UShort && el.type.baseType != VarType::SShort &&
               el.type.baseType != VarType::UByte && el.type.baseType != VarType::SByte)
            {
              reportError(tr("UNORM packing is only supported on [u]byte and [u]short types."));
              success = false;
              break;
            }
          }

          el.type.flags |= ShaderVariableFlags::UNorm;
        }
        else if(annot.name == lit("snorm"))
        {
          if(!(el.type.flags & ShaderVariableFlags::R10G10B10A2))
          {
            // verify that we're integer typed and 1 or 2 bytes
            if(el.type.baseType != VarType::UShort && el.type.baseType != VarType::SShort &&
               el.type.baseType != VarType::UByte && el.type.baseType != VarType::SByte)
            {
              reportError(tr("SNORM packing is only supported on [u]byte and [u]short types."));
              success = false;
              break;
            }
          }

          el.type.flags |= ShaderVariableFlags::SNorm;
        }
        else if(annot.name == lit("row_major"))
        {
          if(el.type.rows == 1)
          {
            reportError(tr("Row major can only be specified on matrices."));
            success = false;
            break;
          }

          el.type.flags |= ShaderVariableFlags::RowMajorMatrix;
        }
        else if(annot.name == lit("col_major"))
        {
          if(el.type.rows == 1)
          {
            reportError(tr("Column major can only be specified on matrices."));
            success = false;
            break;
          }

          el.type.flags &= ~ShaderVariableFlags::RowMajorMatrix;
        }
        else if(annot.name == lit("packed"))
        {
          // already processed
        }
        else if(annot.name == lit("offset") || annot.name == lit("byte_offset"))
        {
          if(annot.param.isEmpty())
          {
            reportError(tr("Annotation '%1' requires a parameter with the offset in bytes.\n\n"
                           "e.g. [[%1(128)]]")
                            .arg(annot.name));
            success = false;
            break;
          }

          uint32_t specifiedOffset = annot.param.toUInt();

          if(specifiedOffset < cur->offset)
          {
            reportError(tr("Specified byte offset %1 overlaps with previous data.\n"
                           "This value must be at byte offset %2 at minimum.")
                            .arg(specifiedOffset)
                            .arg(cur->offset));
            success = false;
            break;
          }

          cur->offset = specifiedOffset;
        }
        else if(annot.name == lit("pad") || annot.name == lit("padding"))
        {
          isPadding = true;
        }
        else if(annot.name == lit("single") || annot.name == lit("fixed"))
        {
          if(cur != &root)
          {
            reportError(tr("[[single]] can only be used on global variables."));
            success = false;
            break;
          }
          else if(!cur->structDef.type.members.empty())
          {
            reportError(
                tr("[[single]] can only be used if there is only one variable in the root.\n"
                   "Consider wrapping the variables in a struct and marking it as [[single]]."));
            success = false;
            break;
          }
          else
          {
            cur->singleMember = true;
          }
        }
        else
        {
          reportError(tr("Unrecognised annotation '%1' on variable.").arg(annot.name));
          success = false;
          break;
        }
      }

      annotations.clear();

      if(!success)
        break;

      // validate that bitfields are only allowed for regular scalars
      if(el.bitFieldSize > 0)
      {
        if(el.type.rows > 1 || el.type.columns > 1)
        {
          reportError(tr("Vectors and matrices can't be packed into a bitfield."));
          success = false;
          break;
        }
        if(el.type.elements > 1)
        {
          reportError(tr("Arrays can't be packed into a bitfield."));
          success = false;
          break;
        }
        if(el.type.flags & (ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::R11G11B10 |
                            ShaderVariableFlags::UNorm | ShaderVariableFlags::SNorm))
        {
          reportError(tr("Format-packed variables can't be packed into a bitfield."));
          success = false;
          break;
        }
        if(VarTypeCompType(el.type.baseType) == CompType::Float)
        {
          reportError(tr("Floating point variables can't be packed into a bitfield."));
          success = false;
          break;
        }
      }

      if(basetype == lit("xlong") || basetype == lit("xint") || basetype == lit("xshort") ||
         basetype == lit("xbyte"))
        el.type.flags |= ShaderVariableFlags::HexDisplay;
    }

    if(cur->singleMember && el.type.elements == ~0U)
    {
      reportError(tr("[[single]] can't be used on unbounded arrays."));
      success = false;
      break;
    }

    const bool packed32bit =
        bool(el.type.flags & (ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::R11G11B10));

    // normally the array stride is the size of an element
    const uint32_t elAlignment = packed32bit ? sizeof(uint32_t) : GetAlignment(pack, el);

    const uint8_t vecSize = (el.type.rows > 1 && el.type.ColMajor()) ? el.type.rows : el.type.columns;

    const uint32_t elSize =
        packed32bit ? sizeof(uint32_t)
                    : (pack.vector_align_component ? elAlignment * vecSize : elAlignment);

    // if we aren't using tight arrays the stride is at least 16 bytes
    el.type.arrayByteStride = elAlignment;
    if(el.type.rows > 1 || el.type.columns > 1)
      el.type.arrayByteStride = elSize;

    if(!pack.tight_arrays)
      el.type.arrayByteStride = std::max(16U, el.type.arrayByteStride);

    // matrices are always aligned like arrays of vectors
    if(el.type.rows > 1)
    {
      // the alignment calculated above is the alignment of a vector, that's our matrix stride
      el.type.matrixByteStride = el.type.arrayByteStride;

      // the array stride is that alignment times the number of rows/columns
      if(el.type.RowMajor())
        el.type.arrayByteStride *= el.type.rows;
      else
        el.type.arrayByteStride *= el.type.columns;
    }

    if(el.bitFieldSize > 0)
    {
      // we can use the elAlignment since this is a scalar so no vector/arrays, this is just the
      // base size. It also works for enums as this is the byte size of the declared underlying type
      const uint32_t elemScalarBitSize = elAlignment * 8;

      // bitfields can't be larger than the base type
      if(el.bitFieldSize > elemScalarBitSize)
      {
        reportError(tr("Variable type %1 only has %2 bits, can't pack into %3 bits in a bitfield.")
                        .arg(el.type.name)
                        .arg(elemScalarBitSize)
                        .arg(el.bitFieldSize));
        success = false;
        break;
      }

      uint32_t start = bitfieldCurPos;
      if(start == ~0U)
        start = 0;

      // if we would end past the current base type size, first roll over and start at the next
      // byte
      // this could be:
      //  unsigned int a : 24;
      //  unsigned byte b : 4;
      //  unsigned byte c : 4;
      // where we just 'rollover' the 3 bytes packed into the unsigned int and start the byte on
      // the next byte, there's no extra padding added
      // or it could be:
      //  unsigned int a : 29;
      //  unsigned byte b : 4;
      //  unsigned byte c : 4;
      // where b would pass through the end of the fourth byte so there ends up being 3 bits of
      // padding between a and b when b is rolled onto the next byte
      // similarly this can happen if the types are the same:
      //  unsigned int a : 29;
      //  unsigned int b : 4;
      // since b would still pass through the end of the first dword.
      // similarly this allows 'more' padding when the types are bigger:
      //  unsigned int a : 17;
      //  unsigned int b : 17;
      // which would produce 15 bytes of padding
      // Note that if the types are the same and big enough we won't roll over, as in:
      //  unsigned int a : 24;
      //  unsigned int b : 4;
      //  unsigned int c : 4;
      if(start + el.bitFieldSize > elemScalarBitSize)
      {
        if(pack.tight_bitfield_packing)
        {
          while(bitfieldCurPos >= 8)
          {
            bitfieldCurPos -= 8;
            cur->offset++;
          }
        }
        else
        {
          // align the offset up to where this bitfield needs to start
          cur->offset += ((bitfieldCurPos + (elemScalarBitSize - 1)) / elemScalarBitSize) *
                         (elemScalarBitSize / 8);
          // reset the current bitfield pos
          bitfieldCurPos = 0;
        }
      }

      // if there's no previous bitpacking, nothing much to do
      if(bitfieldCurPos == ~0U)
      {
        // start the next bitfield at our size
        bitfieldCurPos = el.bitFieldSize;
      }
      else
      {
        // start the next bitfield at the end of the previous
        el.bitFieldOffset = bitfieldCurPos;
        // update by our size
        bitfieldCurPos += el.bitFieldSize;
      }
    }
    else
    {
      // this element is not bitpacked

      if(bitfieldCurPos != ~0U)
      {
        // update offset to account for any bits consumed by the previous bitfield, which won't have
        // happened yet, including any bits in the last byte that weren't allocated
        cur->offset += (bitfieldCurPos + 7) / 8;

        // reset bitpacking state.
        bitfieldCurPos = ~0U;
      }

      // align to our element's base alignment
      cur->offset = AlignUp(cur->offset, elAlignment);

      // if we have non-tight arrays, arrays (and matrices) always start on a 16-byte boundary
      if(!pack.tight_arrays && (el.type.elements > 1 || el.type.rows > 1))
        cur->offset = AlignUp(cur->offset, 16U);

      // if vectors can't straddle 16-byte alignment, check to see if we're going to do that
      if(!pack.vector_straddle_16b)
      {
        if(cur->offset / 16 != (cur->offset + elSize - 1) / 16)
        {
          cur->offset = AlignUp(cur->offset, 16U);
        }
      }
    }

    el.byteOffset = cur->offset;

    if(!isPadding)
    {
      cur->structDef.type.members.push_back(el);
      cur->lineMemberDefs.push_back(line);
    }

    // if we're bitfield packing don't advance offset, otherwise advance to the end of this element
    if(bitfieldCurPos == ~0U)
      cur->offset += GetVarAdvance(pack, el);
  }

  if(bitfieldCurPos != ~0U)
  {
    // update final offset to account for any bits consumed by a trailing bitfield, including any
    // bits in the last byte that weren't allocated
    cur->offset += (bitfieldCurPos + 7) / 8;

    // reset bitpacking state.
    bitfieldCurPos = ~0U;
  }

  ShaderConstant &fixed = ret.fixed;

  // if we succeeded parsing but didn't get any root elements, use the last defined struct as the
  // definition
  if(success && root.structDef.type.members.isEmpty() && !lastStruct.isEmpty())
  {
    root = structelems[lastStruct];

    // only pad up to the stride, not down
    if(root.paddedStride >= root.offset)
      root.offset = cur->paddedStride;
  }

  fixed = root.structDef;
  uint32_t end = root.offset;
  if(!fixed.type.members.isEmpty() &&
     (!pack.tight_bitfield_packing || fixed.type.members.back().bitFieldSize == 0))
    end = qMax(
        end, fixed.type.members.back().byteOffset + GetVarSizeAndTrail(fixed.type.members.back()));

  fixed.type.arrayByteStride = AlignUp(end, GetAlignment(pack, fixed));

  if(!fixed.type.members.isEmpty() && fixed.type.members.back().type.elements == ~0U)
  {
    fixed.type.arrayByteStride =
        AlignUp(fixed.type.members.back().type.arrayByteStride, GetAlignment(pack, fixed));
  }

  if(success)
  {
    // check that unbounded arrays are only the last member of each struct. Doing this separately
    // makes the below check easier since we only have to consider last members
    if(!CheckInvalidUnbounded(root, structelems, errors))
      success = false;
  }

  // we only allow one 'infinite' array. You can't have an infinite member inside an already
  // infinite struct. E.g. this is invalid:
  //
  // struct foo {
  //    uint a;
  //    float b[];
  // };
  //
  // foo data[];
  //
  // but it's valid to have either this:
  //
  // struct foo {
  //    uint a;
  //    float b[3];
  // };
  //
  // foo data[];
  //
  // or this:
  //
  // struct foo {
  //    uint a;
  //    float b[];
  // };
  //
  // foo data;
  if(success)
  {
    ShaderConstant *iter = &fixed;
    ShaderConstant *parent = NULL;
    bool foundInfinite = false;
    int infiniteArrayLine = -1;

    while(iter)
    {
      if(iter->type.elements == ~0U)
      {
        if(foundInfinite)
        {
          QString parentName;

          if(parent)
            parentName = parent->type.name;

          success = false;
          errors[infiniteArrayLine] = tr("Can't declare an unbounded array when child member %1 of "
                                         "struct %2 is also declared as unbounded.")
                                          .arg(iter->name)
                                          .arg(parentName);
          break;
        }

        foundInfinite = true;

        if(parent && parent != &fixed)
          infiniteArrayLine = structelems[parent->type.name].lineMemberDefs.back();
        else
          infiniteArrayLine = root.lineMemberDefs.back();
      }

      // if there are no more members, stop looking
      if(iter->type.members.empty())
        break;

      parent = iter;
      iter = &iter->type.members.back();
    }
  }

  // on D3D if we have an unbounded array it *must* be the root element, as D3D does not support
  // some fixed elements before it, structured buffers are strictly just an AoS.
  // we do allow specifying cbuffers which are all fixed and not unbounded, so we just check to see
  // that if there is an unbounded array that it's the root
  if(
      // on D3D
      IsD3D(m_API) &&
      // if the parsing worked
      success && !fixed.type.members.empty() &&
      // if we have an unbounded array somewhere (we know there's only one, from above)
      ContainsUnbounded(fixed) &&
      // it must be in the root and it must be alone with no siblings
      !(fixed.type.members.size() == 1 && fixed.type.members[0].type.elements == ~0U))
  {
    errors[root.lineMemberDefs.back()] =
        tr("On D3D an unbounded array must be only be used alone as the root element.\n"
           "Consider wrapping all the globals in a single struct, or removing the unbounded array "
           "declaration.");
    success = false;
  }

  // when not viewing a cbuffer, if the root hasn't been explicitly marked as a single struct and we
  // don't have an unbounded array then consider it an AoS definition in all other cases as that is
  // very likely what the user expects
  if(success && !fixed.type.members.empty() && !ContainsUnbounded(fixed) && !root.singleMember &&
     !root.singleDef && !cbuffer)
  {
    // if there's already only one root member just make it infinite
    if(fixed.type.members.size() == 1)
    {
      fixed.type.members[0].type.elements = ~0U;
    }
    else
    {
      // otherwise wrap a struct around the members, to be the infinite AoS
      rdcarray<ShaderConstant> inners;
      inners.swap(fixed.type.members);

      ShaderConstant el;
      el.byteOffset = 0;
      el.type.baseType = VarType::Struct;
      el.type.elements = ~0U;
      el.type.arrayByteStride = fixed.type.arrayByteStride;

      fixed.type.members.push_back(el);
      inners.swap(fixed.type.members[0].type.members);
    }
  }

  if(!success || fixed.type.members.isEmpty())
  {
    fixed.type.members.clear();
    fixed.type.baseType = VarType::Struct;

    ShaderConstant el;
    el.byteOffset = 0;
    el.type.flags = ShaderVariableFlags::RowMajorMatrix | ShaderVariableFlags::HexDisplay;
    el.name = "data";
    el.type.name = "uint4";
    el.type.baseType = VarType::UInt;
    el.type.columns = 4;
    el.type.elements = ~0U;

    if(maxLen > 0 && maxLen < 16)
      el.type.columns = 1;
    if(maxLen > 0 && maxLen < 4)
      el.type.baseType = VarType::UByte;

    el.type.arrayByteStride = el.type.columns * VarTypeByteSize(el.type.baseType);

    fixed.type.members.push_back(el);
    fixed.type.arrayByteStride = el.type.arrayByteStride;
  }

  // split the struct definition we have now into fixed and repeating. We've enforced above that
  // there's only one struct which is unbounded (no other children at any level are unbounded) and
  // it's the last member, so we find it, remove it from the hierarchy, and present it separately.

  {
    ShaderConstant *iter = &fixed;
    rdcstr addedprefix;

    while(iter)
    {
      // if there are no more members, stop looking, there's no repeated member
      if(iter->type.members.empty())
        break;

      // add the prefix, so that the repeated element that's a child like buffer { foo { blah[] } }
      // shows up as buffer.foo.blah
      if(!iter->name.empty())
        addedprefix += iter->name + ".";

      // we want to search the members so we can remove from the current iter
      if(iter->type.members.back().type.elements == ~0U)
      {
        ret.repeating = iter->type.members.back();
        ret.repeating.name = addedprefix + ret.repeating.name;
        ret.repeating.type.elements = 1;
        iter->type.members.pop_back();
        break;
      }

      iter = &iter->type.members.back();
    }
  }

  return ret;
}

QString BufferFormatter::GetTextureFormatString(const TextureDescription &tex)
{
  QString baseType;

  QString varName = lit("pixels");

  uint32_t w = tex.width;

  switch(tex.format.type)
  {
    case ResourceFormatType::BC1:
    case ResourceFormatType::BC2:
    case ResourceFormatType::BC3:
    case ResourceFormatType::BC4:
    case ResourceFormatType::BC5:
    case ResourceFormatType::BC6:
    case ResourceFormatType::BC7:
    case ResourceFormatType::ETC2:
    case ResourceFormatType::EAC:
    case ResourceFormatType::ASTC:
    case ResourceFormatType::PVRTC:
      varName = lit("block");
      // display a 4x4 block at a time
      w /= 4;
    default: break;
  }

  switch(tex.format.type)
  {
    case ResourceFormatType::Regular:
    {
      if(tex.format.compByteWidth == 1)
      {
        if(tex.format.compType == CompType::UNorm || tex.format.compType == CompType::UNormSRGB)
          baseType = lit("[[unorm]] ubyte");
        else if(tex.format.compType == CompType::SNorm)
          baseType = lit("[[snorm]] byte");
        else if(tex.format.compType == CompType::SInt)
          baseType = lit("byte");
        else
          baseType = lit("ubyte");
      }
      else if(tex.format.compByteWidth == 2)
      {
        if(tex.format.compType == CompType::UNorm || tex.format.compType == CompType::UNormSRGB)
          baseType = lit("[[unorm]] ushort");
        else if(tex.format.compType == CompType::SNorm)
          baseType = lit("[[snorm]] short");
        else if(tex.format.compType == CompType::Float)
          baseType = lit("half");
        else if(tex.format.compType == CompType::SInt)
          baseType = lit("short");
        else
          baseType = lit("ushort");
      }
      else if(tex.format.compByteWidth == 4)
      {
        if(tex.format.compType == CompType::Float)
          baseType = lit("float");
        else if(tex.format.compType == CompType::SInt)
          baseType = lit("int");
        else
          baseType = lit("uint");
      }
      else
      {
        if(tex.format.compType == CompType::Float)
          baseType = lit("double");
        else if(tex.format.compType == CompType::SInt)
          baseType = lit("long");
        else
          baseType = lit("ulong");
      }

      baseType = QFormatStr("[[rgb]] %1%2").arg(baseType).arg(tex.format.compCount);

      break;
    }
    // 2x4 byte block, for 64-bit block formats
    case ResourceFormatType::BC1:
    case ResourceFormatType::BC4:
    case ResourceFormatType::ETC2:
    case ResourceFormatType::EAC:
    case ResourceFormatType::PVRTC: baseType = lit("[[hex]] int2"); break;
    // 4x4 byte block, for 128-bit block formats
    case ResourceFormatType::BC2:
    case ResourceFormatType::BC3:
    case ResourceFormatType::BC5:
    case ResourceFormatType::BC6:
    case ResourceFormatType::BC7:
    case ResourceFormatType::ASTC: baseType = lit("[[hex]] int4"); break;
    case ResourceFormatType::R10G10B10A2:
      baseType = lit("[[packed(r10g10b10a2)]] ");
      if(tex.format.compType == CompType::UNorm)
        baseType += lit("[[unorm]] ");
      baseType += lit("uint4");
      break;
    case ResourceFormatType::R11G11B10:
      baseType = lit("[[rgb]] [[packed(r11g11b10)]] float3");
      break;
    case ResourceFormatType::R5G6B5:
    case ResourceFormatType::R5G5B5A1: baseType = lit("[[hex]] short"); break;
    case ResourceFormatType::R9G9B9E5: baseType = lit("[[hex]] int"); break;
    case ResourceFormatType::R4G4B4A4: baseType = lit("[[hex]] short"); break;
    case ResourceFormatType::R4G4: baseType = lit("[[hex]] byte"); break;
    case ResourceFormatType::D16S8:
    case ResourceFormatType::D24S8:
    case ResourceFormatType::D32S8:
    case ResourceFormatType::YUV8: baseType = lit("[[hex]] byte4"); break;
    case ResourceFormatType::YUV10:
    case ResourceFormatType::YUV12:
    case ResourceFormatType::YUV16: baseType = lit("[[hex]] short4"); break;
    case ResourceFormatType::A8:
    case ResourceFormatType::S8:
    case ResourceFormatType::Undefined: baseType = lit("[[hex]] byte"); break;
  }

  if(tex.type == TextureType::Buffer)
    return QFormatStr("%1 %2;").arg(baseType).arg(varName);

  return lit("#pack(scalar) // texture formats are tightly packed\n\n"
             "struct row\n{\n  %1 %2[%3];\n};\n\nrow r[];")
      .arg(baseType)
      .arg(varName)
      .arg(w);
}

QString BufferFormatter::GetBufferFormatString(Packing::Rules pack, ResourceId shader,
                                               const ShaderResource &res,
                                               const ResourceFormat &viewFormat)
{
  QString format;

  if(!res.variableType.members.empty())
  {
    QString structName = res.variableType.name;

    if(structName.isEmpty())
      structName = lit("el");

    QList<QString> declaredStructs;
    QMap<ShaderConstant, QString> anonStructs;
    format = DeclareStruct(pack, shader, declaredStructs, anonStructs, structName,
                           res.variableType.members, 0, QString());
    format = QFormatStr("%1\n\n%2").arg(DeclarePacking(pack)).arg(format);
  }
  else
  {
    const ShaderConstantType &desc = res.variableType;

    if(viewFormat.type == ResourceFormatType::Undefined)
    {
      if(desc.baseType == VarType::Unknown)
      {
        format = desc.name;
      }
      else
      {
        if(desc.RowMajor() && desc.rows > 1 && desc.columns > 1)
          format += lit("[[row_major]] ");

        format += ToQStr(desc.baseType);
        if(desc.rows > 1 && desc.columns > 1)
          format += QFormatStr("%1x%2").arg(desc.rows).arg(desc.columns);
        else if(desc.columns > 1)
          format += QString::number(desc.columns);

        if(!desc.name.empty())
          format += lit(" ") + desc.name;

        if(desc.elements > 1)
          format += QFormatStr("[%1]").arg(desc.elements);
      }
    }
    else
    {
      if(viewFormat.type == ResourceFormatType::R10G10B10A2)
      {
        if(viewFormat.compType == CompType::UInt)
          format = lit("[[packed(r10g10b10a2)]] uint4");
        if(viewFormat.compType == CompType::UNorm)
          format = lit("[[packed(r10g10b10a2)]] [[unorm]] uint4");
      }
      else if(viewFormat.type == ResourceFormatType::R11G11B10)
      {
        format = lit("[[packed(r11g11b10)]] float3");
      }
      else
      {
        switch(viewFormat.compByteWidth)
        {
          case 1:
          {
            if(viewFormat.compType == CompType::UNorm || viewFormat.compType == CompType::UNormSRGB)
              format = lit("[[unorm]] ubyte");
            if(viewFormat.compType == CompType::SNorm)
              format = lit("[[snorm]] byte");
            if(viewFormat.compType == CompType::UInt)
              format = lit("ubyte");
            if(viewFormat.compType == CompType::SInt)
              format = lit("byte");
            break;
          }
          case 2:
          {
            if(viewFormat.compType == CompType::UNorm || viewFormat.compType == CompType::UNormSRGB)
              format = lit("[[unorm]] ushort");
            if(viewFormat.compType == CompType::SNorm)
              format = lit("[[snorm]] short");
            if(viewFormat.compType == CompType::UInt)
              format = lit("ushort");
            if(viewFormat.compType == CompType::SInt)
              format = lit("short");
            if(viewFormat.compType == CompType::Float)
              format = lit("half");
            break;
          }
          case 4:
          {
            if(viewFormat.compType == CompType::UNorm || viewFormat.compType == CompType::UNormSRGB)
              format = lit("unormf");
            if(viewFormat.compType == CompType::SNorm)
              format = lit("snormf");
            if(viewFormat.compType == CompType::UInt)
              format = lit("uint");
            if(viewFormat.compType == CompType::SInt)
              format = lit("int");
            if(viewFormat.compType == CompType::Float)
              format = lit("float");
            break;
          }
        }

        format += QString::number(viewFormat.compCount);
      }
    }
  }

  return format;
}

uint32_t BufferFormatter::GetVarStraddleSize(const ShaderConstant &var)
{
  if(var.type.baseType == VarType::Enum)
    return var.type.matrixByteStride;

  // structs don't themselves have a straddle size
  // this is fine because the struct members itself don't straddle, and the alignment of the max of
  // their members. A struct that contains a vector will have to satisfy that vector's alignment. on
  // std140/430 this means the struct will be aligned such that as long as its members don't
  // straddle then any aligned placement of the struct they also won't straddle.
  // for D3D cbuffers where vectors have float alignment, structs are aligned to 16 always.
  // all others are scalar so straddling is allowed - i.e there is no packing scheme that disallows
  // straddling but allows vector AND struct placement so freely that a vector member could avoid
  // straddling until the struct is placed at a particular offset.
  if(!var.type.members.empty())
    return 0;

  if(var.type.rows > 1)
    return var.type.matrixByteStride;

  return VarTypeByteSize(var.type.baseType) * var.type.columns;
}

uint32_t BufferFormatter::GetVarSizeAndTrail(const ShaderConstant &var)
{
  if(var.type.elements > 1 && var.type.elements != ~0U)
    return var.type.arrayByteStride * var.type.elements;

  if(var.type.baseType == VarType::Enum)
    return var.type.matrixByteStride;

  if(!var.type.members.empty())
    return var.type.arrayByteStride;

  if(var.type.rows > 1)
  {
    if(var.type.RowMajor())
      return var.type.matrixByteStride * var.type.rows;
    else
      return var.type.matrixByteStride * var.type.columns;
  }

  return VarTypeByteSize(var.type.baseType) * var.type.columns;
}

uint32_t BufferFormatter::GetVarAdvance(const Packing::Rules &pack, const ShaderConstant &var)
{
  uint32_t ret = GetVarSizeAndTrail(var);

  // if we allow trailing overlap, remove the padding at the end of the struct/array
  if(pack.trailing_overlap)
  {
    if(var.type.baseType == VarType::Struct)
    {
      ret -= (var.type.arrayByteStride - GetUnpaddedStructAdvance(pack, var.type.members));
    }
    else if((var.type.elements > 1 || var.type.rows > 1) && !pack.tight_arrays)
    {
      uint8_t vecSize = var.type.columns;

      if(var.type.rows > 1 && var.type.ColMajor())
        vecSize = var.type.rows;

      uint32_t elSize = GetAlignment(pack, var);
      if(pack.vector_align_component)
        elSize *= vecSize;

      // the padding is the stride (which is rounded up to 16 for non-tight arrays) minus the size
      // of the last vector (whether or not this is an array of scalars, vectors or matrices
      ret -= 16 - elSize;
    }
  }

  return ret;
}

uint32_t BufferFormatter::GetAlignment(Packing::Rules pack, const ShaderConstant &c)
{
  uint32_t ret = 1;

  if(c.type.baseType == VarType::Struct)
  {
    for(const ShaderConstant &m : c.type.members)
      ret = std::max(ret, GetAlignment(pack, m));
  }
  else if(c.type.baseType == VarType::Enum)
  {
    ret = c.type.matrixByteStride;
  }
  else if(c.type.members.empty())
  {
    uint32_t align = VarTypeByteSize(c.type.baseType);

    // if vectors aren't component aligned we need to calculate the alignment based on the size of
    // the vectors
    if(!pack.vector_align_component)
    {
      // column major matrices have vectors that are 'rows' long. Everything else is vectors of
      // 'columns' long
      uint8_t vecSize = c.type.columns;

      if(c.type.rows > 1 && c.type.ColMajor())
        vecSize = c.type.rows;

      // 3- and 4- vectors are 4-component aligned
      if(vecSize >= 3)
        align *= 4;

      // 2- vectors are 2-component aligned
      else if(vecSize == 2)
        align *= 2;
    }

    ret = std::max(ret, align);
  }

  return ret;
}

uint32_t BufferFormatter::GetUnpaddedStructAdvance(Packing::Rules pack,
                                                   const rdcarray<ShaderConstant> &members)
{
  uint32_t lastMemberStart = 0;

  if(members.empty())
    return 0;

  const ShaderConstant *lastChild = &members.back();

  lastMemberStart += lastChild->byteOffset;
  while(lastChild->type.baseType != VarType::Enum && !lastChild->type.members.isEmpty())
  {
    if(lastChild->type.elements != ~0U)
      lastMemberStart += (qMax(lastChild->type.elements, 1U) - 1) * lastChild->type.arrayByteStride;
    lastChild = &lastChild->type.members.back();
    lastMemberStart += lastChild->byteOffset;
  }

  return lastMemberStart + GetVarAdvance(pack, *lastChild);
}

QString BufferFormatter::DeclareStruct(Packing::Rules pack, ResourceId shader,
                                       QList<QString> &declaredStructs,
                                       QMap<ShaderConstant, QString> &anonStructs,
                                       const QString &name, const rdcarray<ShaderConstant> &members,
                                       uint32_t requiredByteStride, QString innerSkippedPrefixString)
{
  QString declarations;

  QString ret;

  ret = lit("struct %1\n{\n").arg(MakeIdentifierName(name));

  ret += innerSkippedPrefixString;

  uint32_t offset = 0;

  uint32_t structAlignment = 1;

  for(int i = 0; i < members.count(); i++)
  {
    const uint32_t alignment = GetAlignment(pack, members[i]);
    const uint32_t vecsize = GetVarStraddleSize(members[i]);
    structAlignment = std::max(structAlignment, alignment);

    offset = AlignUp(offset, alignment);

    // if things can't straddle 16-byte boundaries, check that and enforce
    if(!pack.vector_straddle_16b)
    {
      if(offset / 16 != (offset + vecsize - 1) / 16)
        offset = AlignUp(offset, 16U);
    }

    // if we don't have tight arrays, arrays and structs begin at 16-byte boundaries
    if(!pack.tight_arrays && (members[i].type.baseType == VarType::Struct ||
                              members[i].type.elements > 1 || members[i].type.rows > 1))
    {
      offset = AlignUp(offset, 16U);
    }

    // if this variable is placed later, add an offset annotation
    if(offset < members[i].byteOffset)
      ret += lit("    [[offset(%1)]]\n").arg(members[i].byteOffset);
    else if(offset > members[i].byteOffset)
      qCritical() << "Unexpected offset overlap at" << QString(members[i].name) << "in"
                  << QString(name);

    offset = members[i].byteOffset;

    offset += GetVarAdvance(pack, members[i]);

    QString arraySize;
    if(members[i].type.elements > 1)
    {
      if(members[i].type.elements != ~0U)
        arraySize = QFormatStr("[%1]").arg(members[i].type.elements);
      else
        arraySize = lit("[]");
    }

    QString varTypeName = MakeIdentifierName(members[i].type.name);

    if(members[i].type.pointerTypeID != ~0U)
    {
      const ShaderConstantType &pointeeType =
          PointerTypeRegistry::GetTypeDescriptor(shader, members[i].type.pointerTypeID);

      varTypeName = MakeIdentifierName(pointeeType.name);

      if(!declaredStructs.contains(varTypeName))
      {
        declaredStructs.push_back(varTypeName);
        declarations += DeclareStruct(pack, shader, declaredStructs, anonStructs, varTypeName,
                                      pointeeType.members, pointeeType.arrayByteStride, QString()) +
                        lit("\n");
      }

      varTypeName += lit("*");
    }
    else if(members[i].type.baseType == VarType::Struct)
    {
      // GL structs don't give us typenames (boo!) so give them unique names. This will mean some
      // structs get duplicated if they're used in multiple places, but not much we can do about
      // that.
      if(varTypeName.isEmpty() || varTypeName == lit("struct"))
      {
        ShaderConstant key;
        key.type.members = members[i].type.members;

        if(anonStructs.contains(key))
        {
          varTypeName = anonStructs[key];
        }
        else
        {
          varTypeName = anonStructs[key] = lit("struct%1").arg(anonStructs.size() + 1);
        }
      }

      if(!declaredStructs.contains(varTypeName))
      {
        declaredStructs.push_back(varTypeName);
        declarations +=
            DeclareStruct(pack, shader, declaredStructs, anonStructs, varTypeName,
                          members[i].type.members, members[i].type.arrayByteStride, QString()) +
            lit("\n");
      }
    }

    QString varName = MakeIdentifierName(members[i].name);

    if(varName.isEmpty())
      varName = QFormatStr("_child%1").arg(i);

    if(members[i].type.rows > 1)
    {
      if(members[i].type.RowMajor())
        varTypeName = lit("[[row_major]] ") + varTypeName;

      uint32_t stride = GetAlignment(pack, members[i]);

      if(pack.vector_align_component)
      {
        if(members[i].type.RowMajor())
          stride *= members[i].type.columns;
        else
          stride *= members[i].type.rows;
      }

      if(!pack.tight_arrays)
        stride = 16;

      if(stride != members[i].type.matrixByteStride)
        ret += lit("// unexpected matrix stride %1").arg(members[i].type.matrixByteStride);
    }

    ret += QFormatStr("    %1 %2%3;\n").arg(varTypeName).arg(varName).arg(arraySize);
  }

  // if we don't have tight arrays, struct byte strides are always 16-byte aligned
  if(!pack.tight_arrays)
  {
    structAlignment = 16;
  }

  offset = AlignUp(offset, structAlignment);

  if(requiredByteStride > 0)
  {
    if(requiredByteStride > offset)
      ret = lit("[[size(%1)]]\n%2").arg(requiredByteStride).arg(ret);
    else if(requiredByteStride != offset)
      ret = lit("// Unexpected size of struct %1\n%2").arg(requiredByteStride).arg(ret);
  }

  ret += lit("}\n");

  return declarations + ret;
}

QString BufferFormatter::DeclareStruct(Packing::Rules pack, ResourceId shader, const QString &name,
                                       const rdcarray<ShaderConstant> &members,
                                       uint32_t requiredByteStride)
{
  QList<QString> declaredStructs;
  QMap<ShaderConstant, QString> anonStructs;
  QString structDef = DeclareStruct(pack, shader, declaredStructs, anonStructs, name, members,
                                    requiredByteStride, QString());

  return QFormatStr("%1\n\n%2").arg(DeclarePacking(pack)).arg(structDef);
}

ResourceFormat GetInterpretedResourceFormat(const ShaderConstant &elem)
{
  ResourceFormat format;
  format.type = ResourceFormatType::Regular;

  if(elem.type.flags & ShaderVariableFlags::R10G10B10A2)
    format.type = ResourceFormatType::R10G10B10A2;
  else if(elem.type.flags & ShaderVariableFlags::R11G11B10)
    format.type = ResourceFormatType::R11G11B10;

  format.compType = VarTypeCompType(elem.type.baseType);

  if(elem.type.flags & ShaderVariableFlags::UNorm)
    format.compType = CompType::UNorm;
  else if(elem.type.flags & ShaderVariableFlags::SNorm)
    format.compType = CompType::SNorm;

  format.compByteWidth = VarTypeByteSize(elem.type.baseType);

  if(elem.type.baseType == VarType::Enum)
    format.compByteWidth = elem.type.matrixByteStride;

  if(elem.type.RowMajor() || elem.type.rows == 1)
    format.compCount = elem.type.columns;
  else
    format.compCount = elem.type.rows;

  if(elem.type.baseType == VarType::GPUPointer)
  {
    format.compCount = 1;
    format.compType = CompType::UInt;
    format.compByteWidth = 8;
  }

  return format;
}

static void FillShaderVarData(ShaderVariable &var, const ShaderConstant &elem, const byte *data,
                              const byte *end)
{
  int src = 0;

  uint32_t outerCount = elem.type.rows;
  uint32_t innerCount = elem.type.columns;

  bool colMajor = false;

  if(elem.type.ColMajor() && outerCount > 1)
  {
    colMajor = true;
    std::swap(outerCount, innerCount);
  }

  QVariantList objs = GetVariants(GetInterpretedResourceFormat(elem), elem, data, end);

  if(objs.isEmpty())
  {
    var.name = elem.name;
    var.value = ShaderValue();
    var.flags |= ShaderVariableFlags::Truncated;
    return;
  }

  for(uint32_t outer = 0; outer < outerCount; outer++)
  {
    for(uint32_t inner = 0; inner < innerCount; inner++)
    {
      uint32_t dst = outer * innerCount + inner;

      QVariant o = objs[src];

      src++;

      switch(var.type)
      {
        case VarType::Float: var.value.f32v[dst] = o.toFloat(); break;
        case VarType::Double: var.value.f64v[dst] = o.toDouble(); break;
        case VarType::Half: var.value.f16v[dst] = rdhalf::make(o.toFloat()); break;
        case VarType::Bool: var.value.u32v[dst] = o.toBool() ? 1 : 0; break;
        case VarType::ULong: var.value.u64v[dst] = o.toULongLong(); break;
        case VarType::UInt: var.value.u32v[dst] = o.toUInt(); break;
        case VarType::UShort: var.value.u16v[dst] = o.toUInt() & 0xffff; break;
        case VarType::UByte: var.value.u8v[dst] = o.toUInt() & 0xff; break;
        case VarType::SLong: var.value.s64v[dst] = o.toLongLong(); break;
        case VarType::SInt: var.value.s32v[dst] = o.toInt(); break;
        case VarType::SShort:
          var.value.u16v[dst] = (int16_t)qBound((int)INT16_MIN, o.toInt(), (int)INT16_MAX);
          break;
        case VarType::SByte:
          var.value.u8v[dst] = (int8_t)qBound((int)INT8_MIN, o.toInt(), (int)INT8_MAX);
          break;
        case VarType::Enum: var.value.u64v[dst] = o.value<EnumInterpValue>().val; break;
        case VarType::GPUPointer:
          var.SetTypedPointer(o.toULongLong(), ResourceId(), elem.type.pointerTypeID);
          break;
        case VarType::ConstantBlock:
        case VarType::ReadOnlyResource:
        case VarType::ReadWriteResource:
        case VarType::Sampler:
        case VarType::Unknown:
        case VarType::Struct:
          qCritical() << "Unexpected variable type" << ToQStr(var.type) << "in variable"
                      << (QString)var.name;
          break;
      }
    }
  }
}

ShaderVariable InterpretShaderVar(const ShaderConstant &elem, const byte *data, const byte *end)
{
  ShaderVariable ret;

  ret.name = elem.name;
  ret.type = elem.type.baseType;
  ret.columns = qMin(elem.type.columns, uint8_t(4));
  ret.rows = qMin(elem.type.rows, uint8_t(4));

  ret.flags = elem.type.flags;

  if(elem.type.baseType != VarType::Enum && !elem.type.members.isEmpty())
  {
    ret.rows = ret.columns = 0;

    if(elem.type.elements > 1 && elem.type.elements != ~0U)
    {
      rdcarray<ShaderVariable> arrayElements;

      for(uint32_t a = 0; a < elem.type.elements; a++)
      {
        rdcarray<ShaderVariable> members;

        for(size_t m = 0; m < elem.type.members.size(); m++)
        {
          const ShaderConstant &member = elem.type.members[m];

          members.push_back(InterpretShaderVar(member, data + member.byteOffset, end));
        }

        arrayElements.push_back(ret);
        arrayElements.back().name = QFormatStr("%1[%2]").arg(ret.name).arg(a);
        arrayElements.back().members = members;

        data += elem.type.arrayByteStride;
      }

      ret.members = arrayElements;
    }
    else
    {
      rdcarray<ShaderVariable> members;

      for(size_t m = 0; m < elem.type.members.size(); m++)
      {
        const ShaderConstant &member = elem.type.members[m];

        members.push_back(InterpretShaderVar(member, data + member.byteOffset, end));
      }

      ret.members = members;
    }
  }
  else if(elem.type.baseType == VarType::Struct && elem.type.members.isEmpty())
  {
  }
  else if(elem.type.elements > 1 && elem.type.elements != ~0U)
  {
    rdcarray<ShaderVariable> arrayElements;

    for(uint32_t a = 0; a < elem.type.elements; a++)
    {
      arrayElements.push_back(ret);
      arrayElements.back().name = QFormatStr("%1[%2]").arg(ret.name).arg(a);
      FillShaderVarData(arrayElements.back(), elem, data, end);
      data += elem.type.arrayByteStride;
    }

    ret.rows = ret.columns = 0;
    ret.members = arrayElements;
  }
  else
  {
    FillShaderVarData(ret, elem, data, end);
  }

  return ret;
}

static QVariant interpret(const ResourceFormat &f, uint16_t comp)
{
  if(f.compByteWidth != 2 || f.compType == CompType::Float)
    return QVariant();

  if(f.compType == CompType::SInt)
  {
    return (int16_t)comp;
  }
  else if(f.compType == CompType::UInt)
  {
    return comp;
  }
  else if(f.compType == CompType::SScaled)
  {
    return (float)((int16_t)comp);
  }
  else if(f.compType == CompType::UScaled)
  {
    return (float)comp;
  }
  else if(f.compType == CompType::UNorm || f.compType == CompType::UNormSRGB)
  {
    return (float)comp / (float)0xffff;
  }
  else if(f.compType == CompType::SNorm)
  {
    int16_t cast = (int16_t)comp;

    float ret = -1.0f;

    if(cast == -32768)
      ret = -1.0f;
    else
      ret = ((float)cast) / 32767.0f;

    return ret;
  }

  return QVariant();
}

static QVariant interpret(const ResourceFormat &f, byte comp)
{
  if(f.compByteWidth != 1 || f.compType == CompType::Float)
    return QVariant();

  if(f.compType == CompType::SInt)
  {
    return (int8_t)comp;
  }
  else if(f.compType == CompType::UInt)
  {
    return comp;
  }
  else if(f.compType == CompType::SScaled)
  {
    return (float)((int8_t)comp);
  }
  else if(f.compType == CompType::UScaled)
  {
    return (float)comp;
  }
  else if(f.compType == CompType::UNorm || f.compType == CompType::UNormSRGB)
  {
    return ((float)comp) / 255.0f;
  }
  else if(f.compType == CompType::SNorm)
  {
    int8_t cast = (int8_t)comp;

    float ret = -1.0f;

    if(cast == -128)
      ret = -1.0f;
    else
      ret = ((float)cast) / 127.0f;

    return ret;
  }

  return QVariant();
}

template <typename T>
inline T readObj(const byte *&data, const byte *end, bool &ok)
{
  if(data + sizeof(T) > end)
  {
    ok = false;
    return T();
  }

  T ret = *(T *)data;

  data += sizeof(T);

  return ret;
}

QVariantList GetVariants(ResourceFormat format, const ShaderConstant &var, const byte *&data,
                         const byte *end)
{
  const ShaderConstantType &varType = var.type;

  QVariantList ret;

  bool ok = true;

  if(format.type == ResourceFormatType::R5G5B5A1)
  {
    uint16_t packed = readObj<uint16_t>(data, end, ok);

    ret.push_back((float)((packed >> 0) & 0x1f) / 31.0f);
    ret.push_back((float)((packed >> 5) & 0x1f) / 31.0f);
    ret.push_back((float)((packed >> 10) & 0x1f) / 31.0f);
    ret.push_back(((packed & 0x8000) > 0) ? 1.0f : 0.0f);

    if(format.BGRAOrder())
    {
      QVariant tmp = ret[2];
      ret[2] = ret[0];
      ret[0] = tmp;
    }
  }
  else if(format.type == ResourceFormatType::R5G6B5)
  {
    uint16_t packed = readObj<uint16_t>(data, end, ok);

    ret.push_back((float)((packed >> 0) & 0x1f) / 31.0f);
    ret.push_back((float)((packed >> 5) & 0x3f) / 63.0f);
    ret.push_back((float)((packed >> 11) & 0x1f) / 31.0f);

    if(format.BGRAOrder())
    {
      QVariant tmp = ret[2];
      ret[2] = ret[0];
      ret[0] = tmp;
    }
  }
  else if(format.type == ResourceFormatType::R4G4B4A4)
  {
    uint16_t packed = readObj<uint16_t>(data, end, ok);

    ret.push_back((float)((packed >> 0) & 0xf) / 15.0f);
    ret.push_back((float)((packed >> 4) & 0xf) / 15.0f);
    ret.push_back((float)((packed >> 8) & 0xf) / 15.0f);
    ret.push_back((float)((packed >> 12) & 0xf) / 15.0f);

    if(format.BGRAOrder())
    {
      QVariant tmp = ret[2];
      ret[2] = ret[0];
      ret[0] = tmp;
    }
  }
  else if(format.type == ResourceFormatType::R10G10B10A2)
  {
    // allow for vectors of this format - for raw buffer viewer
    for(int i = 0; i < int(format.compCount / 4); i++)
    {
      uint32_t packed = readObj<uint32_t>(data, end, ok);

      uint32_t r = (packed >> 0) & 0x3ff;
      uint32_t g = (packed >> 10) & 0x3ff;
      uint32_t b = (packed >> 20) & 0x3ff;
      uint32_t a = (packed >> 30) & 0x003;

      if(format.BGRAOrder())
      {
        uint32_t tmp = b;
        b = r;
        r = tmp;
      }

      if(format.compType == CompType::UInt)
      {
        ret.push_back(r);
        ret.push_back(g);
        ret.push_back(b);
        ret.push_back(a);
      }
      else if(format.compType == CompType::UScaled)
      {
        ret.push_back((float)r);
        ret.push_back((float)g);
        ret.push_back((float)b);
        ret.push_back((float)a);
      }
      else if(format.compType == CompType::SInt || format.compType == CompType::SScaled ||
              format.compType == CompType::SNorm)
      {
        int ir, ig, ib, ia;

        // interpret RGB as 10-bit signed integers
        if(r <= 511)
          ir = (int)r;
        else
          ir = ((int)r) - 1024;

        if(g <= 511)
          ig = (int)g;
        else
          ig = ((int)g) - 1024;

        if(b <= 511)
          ib = (int)b;
        else
          ib = ((int)b) - 1024;

        // 2-bit signed integer
        if(a <= 1)
          ia = (int)a;
        else
          ia = ((int)a) - 4;

        if(format.compType == CompType::SInt)
        {
          ret.push_back(ir);
          ret.push_back(ig);
          ret.push_back(ib);
          ret.push_back(ia);
        }
        else if(format.compType == CompType::SScaled)
        {
          ret.push_back((float)ir);
          ret.push_back((float)ig);
          ret.push_back((float)ib);
          ret.push_back((float)ia);
        }
        else if(format.compType == CompType::SNorm)
        {
          if(ir == -512)
            ir = -511;
          if(ig == -512)
            ig = -511;
          if(ib == -512)
            ib = -511;
          if(ia == -2)
            ia = -1;

          ret.push_back((float)ir / 511.0f);
          ret.push_back((float)ig / 511.0f);
          ret.push_back((float)ib / 511.0f);
          ret.push_back((float)ia / 1.0f);
        }
      }
      else
      {
        ret.push_back((float)r / 1023.0f);
        ret.push_back((float)g / 1023.0f);
        ret.push_back((float)b / 1023.0f);
        ret.push_back((float)a / 3.0f);
      }
    }
  }
  else if(format.type == ResourceFormatType::R11G11B10)
  {
    uint32_t packed = readObj<uint32_t>(data, end, ok);

    uint32_t mantissas[] = {
        (packed >> 0) & 0x3f,
        (packed >> 11) & 0x3f,
        (packed >> 22) & 0x1f,
    };
    int32_t exponents[] = {
        int32_t(packed >> 6) & 0x1f,
        int32_t(packed >> 17) & 0x1f,
        int32_t(packed >> 27) & 0x1f,
    };
    static const uint32_t leadbit[] = {
        0x40,
        0x40,
        0x20,
    };

    for(int i = 0; i < 3; i++)
    {
      if(mantissas[i] == 0 && exponents[i] == 0)
      {
        ret.push_back((float)0.0f);
      }
      else
      {
        if(exponents[i] == 0x1f)
        {
          // no sign bit, can't be negative infinity
          if(mantissas[i] == 0)
            ret.push_back((float)qInf());
          else
            ret.push_back((float)qQNaN());
        }
        else if(exponents[i] != 0)
        {
          // normal value, add leading bit
          uint32_t combined = leadbit[i] | mantissas[i];

          // calculate value
          ret.push_back(((float)combined / (float)leadbit[i]) *
                        qPow(2.0f, (float)exponents[i] - 15.0f));
        }
        else if(exponents[i] == 0)
        {
          // we know xMantissa isn't 0 also, or it would have been caught above so
          // this is a subnormal value, pretend exponent is 1 and don't add leading bit

          ret.push_back(((float)mantissas[i] / (float)leadbit[i]) * qPow(2.0f, (float)1.0f - 15.0f));
        }
      }
    }
  }
  else
  {
    const byte *base = data;

    uint32_t rowCount = varType.rows;
    uint32_t colCount = varType.columns;

    for(uint32_t row = 0; row < qMax(rowCount, 1U); row++)
    {
      for(uint32_t col = 0; col < qMax(colCount, 1U); col++)
      {
        if(varType.RowMajor() || rowCount == 1)
          data = base + row * varType.matrixByteStride + col * format.compByteWidth;
        else
          data = base + col * varType.matrixByteStride + row * format.compByteWidth;

        if(format.compType == CompType::Float)
        {
          if(format.compByteWidth == 8)
            ret.push_back(readObj<double>(data, end, ok));
          else if(format.compByteWidth == 4)
            ret.push_back(readObj<float>(data, end, ok));
          else if(format.compByteWidth == 2)
            ret.push_back((float)rdhalf::make(readObj<uint16_t>(data, end, ok)));
        }
        else if(format.compType == CompType::SInt)
        {
          if(var.bitFieldSize == 0)
          {
            if(format.compByteWidth == 8)
              ret.push_back((qlonglong)readObj<int64_t>(data, end, ok));
            else if(format.compByteWidth == 4)
              ret.push_back((int)readObj<int32_t>(data, end, ok));
            else if(format.compByteWidth == 2)
              ret.push_back((int)readObj<int16_t>(data, end, ok));
            else if(format.compByteWidth == 1)
              ret.push_back((int)readObj<int8_t>(data, end, ok));
          }
          else
          {
            uint64_t uval = 0;
            if(format.compByteWidth == 8)
              uval = readObj<uint64_t>(data, end, ok);
            else if(format.compByteWidth == 4)
              uval = readObj<uint32_t>(data, end, ok);
            else if(format.compByteWidth == 2)
              uval = readObj<uint16_t>(data, end, ok);
            else if(format.compByteWidth == 1)
              uval = readObj<uint8_t>(data, end, ok);

            int64_t val = 0;

            if(ok)
            {
              // shift by the offset
              uval >>= var.bitFieldOffset;

              // mask by the size
              const uint64_t mask = ((1ULL << var.bitFieldSize) - 1ULL);
              uval &= mask;

              // sign extend by hand
              if(uval & (1ULL << (var.bitFieldSize - 1)))
                uval |= ~0ULL ^ mask;

              memcpy(&val, &uval, sizeof(uval));
            }

            if(format.compByteWidth == 8)
              ret.push_back(qlonglong(val));
            else if(format.compByteWidth == 4)
              ret.push_back(int32_t(val));
            else if(format.compByteWidth == 2)
              ret.push_back(int16_t(val));
            else if(format.compByteWidth == 1)
              ret.push_back(int8_t(val));
          }
        }
        else if(format.compType == CompType::UInt)
        {
          if(var.bitFieldSize == 0)
          {
            if(format.compByteWidth == 8)
              ret.push_back((qulonglong)readObj<uint64_t>(data, end, ok));
            else if(format.compByteWidth == 4)
              ret.push_back((uint32_t)readObj<uint32_t>(data, end, ok));
            else if(format.compByteWidth == 2)
              ret.push_back((uint32_t)readObj<uint16_t>(data, end, ok));
            else if(format.compByteWidth == 1)
              ret.push_back((uint32_t)readObj<uint8_t>(data, end, ok));
          }
          else
          {
            uint64_t val = 0;
            if(format.compByteWidth == 8)
              val = readObj<uint64_t>(data, end, ok);
            else if(format.compByteWidth == 4)
              val = readObj<uint32_t>(data, end, ok);
            else if(format.compByteWidth == 2)
              val = readObj<uint16_t>(data, end, ok);
            else if(format.compByteWidth == 1)
              val = readObj<uint8_t>(data, end, ok);

            if(ok)
            {
              // shift by the offset
              val >>= var.bitFieldOffset;

              // mask by the size
              val &= ((1ULL << var.bitFieldSize) - 1ULL);
            }
            else
            {
              val = 0;
            }

            if(format.compByteWidth == 8)
              ret.push_back(qulonglong(val));
            else if(format.compByteWidth == 4)
              ret.push_back(uint32_t(val));
            else if(format.compByteWidth == 2)
              ret.push_back(uint16_t(val));
            else if(format.compByteWidth == 1)
              ret.push_back(uint8_t(val));
          }

          if(var.type.baseType == VarType::Enum && !ret.empty())
          {
            EnumInterpValue newval;
            newval.val = ret.back().toULongLong();

            for(size_t i = 0; i < var.type.members.size(); i++)
            {
              if(newval.val == var.type.members[i].defaultValue)
              {
                newval.str = var.type.members[i].name;
                break;
              }
            }

            if(newval.str.isEmpty())
              newval.str = QApplication::translate("BufferFormatter", "Unknown %1 (%2)")
                               .arg(QString(var.type.name))
                               .arg(newval.val);

            ret.back() = QVariant::fromValue(newval);
          }
        }
        else if(format.compType == CompType::UScaled)
        {
          if(format.compByteWidth == 4)
            ret.push_back((float)readObj<uint32_t>(data, end, ok));
          else if(format.compByteWidth == 2)
            ret.push_back((float)readObj<uint16_t>(data, end, ok));
          else if(format.compByteWidth == 1)
            ret.push_back((float)readObj<uint8_t>(data, end, ok));
        }
        else if(format.compType == CompType::SScaled)
        {
          if(format.compByteWidth == 4)
            ret.push_back((float)readObj<int32_t>(data, end, ok));
          else if(format.compByteWidth == 2)
            ret.push_back((float)readObj<int16_t>(data, end, ok));
          else if(format.compByteWidth == 1)
            ret.push_back((float)readObj<int8_t>(data, end, ok));
        }
        else if(format.compType == CompType::Depth)
        {
          if(format.compByteWidth == 4)
          {
            // 32-bit depth is native floats
            ret.push_back(readObj<float>(data, end, ok));
          }
          else if(format.compByteWidth == 3)
          {
            // 32-bit depth is normalised, masked against non-stencil bits
            uint32_t f = readObj<uint32_t>(data, end, ok);
            f &= 0x00ffffff;
            ret.push_back((float)f / (float)0x00ffffff);
          }
          else if(format.compByteWidth == 2)
          {
            // 16-bit depth is normalised
            float f = (float)readObj<uint16_t>(data, end, ok);
            ret.push_back(f / (float)0x0000ffff);
          }
        }
        else
        {
          // unorm/snorm

          if(format.compByteWidth == 4)
          {
            // should never hit this - no 32bit unorm/snorm type
            qCritical() << "Unexpected 4-byte unorm/snorm value";
            ret.push_back((float)readObj<uint32_t>(data, end, ok) / (float)0xffffffff);
          }
          else if(format.compByteWidth == 2)
          {
            ret.push_back(interpret(format, readObj<uint16_t>(data, end, ok)));
          }
          else if(format.compByteWidth == 1)
          {
            ret.push_back(interpret(format, readObj<uint8_t>(data, end, ok)));
          }
        }
      }
    }

    if(format.BGRAOrder())
    {
      QVariant tmp = ret[2];
      ret[2] = ret[0];
      ret[0] = tmp;
    }
  }

  // we read off the end, return empty set
  if(!ok)
    ret.clear();

  return ret;
}

QString TypeString(const ShaderVariable &v, const ShaderConstant &c)
{
  if(!v.members.isEmpty() || v.type == VarType::Struct || v.type == VarType::Enum)
  {
    if(v.type == VarType::Struct || v.type == VarType::Enum)
    {
      QString structName = v.type == VarType::Struct ? lit("struct") : lit("enum");

      if(c.type.baseType == v.type && !c.type.name.empty())
        structName = c.type.name;

      if(!v.members.empty() && v.members[0].name.contains('['))
        return lit("%1[%2]").arg(structName).arg(v.members.count());
      else
        return lit("%1").arg(structName);
    }
    else
    {
      return QFormatStr("%1[%2]")
          .arg(TypeString(v.members[0], c.type.members.empty() ? c : c.type.members[0]))
          .arg(v.members.count());
    }
  }

  if(v.type == VarType::GPUPointer)
    return PointerTypeRegistry::GetTypeDescriptor(v.GetPointer()).name + "*";

  QString typeStr = ToQStr(v.type);

  if(v.type == VarType::ReadOnlyResource)
    typeStr = lit("Resource");
  else if(v.type == VarType::ReadWriteResource)
    typeStr = lit("RW Resource");
  else if(v.type == VarType::Sampler)
    typeStr = lit("Sampler");
  else if(v.type == VarType::ConstantBlock)
    typeStr = lit("Constant Block");

  if(v.type == VarType::Unknown)
    return lit("Typeless");
  if(v.rows == 1 && v.columns == 1)
    return typeStr;
  if(v.rows == 1)
    return QFormatStr("%1%2").arg(typeStr).arg(v.columns);
  else
    return QFormatStr("%1%2x%3 (%4)")
        .arg(typeStr)
        .arg(v.rows)
        .arg(v.columns)
        .arg(v.RowMajor() ? lit("row_major") : lit("column_major"));
}

template <typename el>
static QString RowValuesToString(int cols, ShaderVariableFlags flags, el x, el y, el z, el w)
{
  if(flags & ShaderVariableFlags::Truncated)
  {
    QString ret = lit("---");
    for(int i = 1; i < cols; i++)
      ret += lit(", ---");
    return ret;
  }

  const bool hex = bool(flags & ShaderVariableFlags::HexDisplay);

  if(bool(flags & ShaderVariableFlags::BinaryDisplay))
  {
    if(cols == 1)
      return Formatter::BinFormat(x);
    else if(cols == 2)
      return QFormatStr("%1, %2").arg(Formatter::BinFormat(x)).arg(Formatter::BinFormat(y));
    else if(cols == 3)
      return QFormatStr("%1, %2, %3")
          .arg(Formatter::BinFormat(x))
          .arg(Formatter::BinFormat(y))
          .arg(Formatter::BinFormat(z));
    else
      return QFormatStr("%1, %2, %3, %4")
          .arg(Formatter::BinFormat(x))
          .arg(Formatter::BinFormat(y))
          .arg(Formatter::BinFormat(z))
          .arg(Formatter::BinFormat(w));
  }

  if(cols == 1)
    return Formatter::Format(x, hex);
  else if(cols == 2)
    return QFormatStr("%1, %2").arg(Formatter::Format(x, hex)).arg(Formatter::Format(y, hex));
  else if(cols == 3)
    return QFormatStr("%1, %2, %3")
        .arg(Formatter::Format(x, hex))
        .arg(Formatter::Format(y, hex))
        .arg(Formatter::Format(z, hex));
  else
    return QFormatStr("%1, %2, %3, %4")
        .arg(Formatter::Format(x, hex))
        .arg(Formatter::Format(y, hex))
        .arg(Formatter::Format(z, hex))
        .arg(Formatter::Format(w, hex));
}

QString RowString(const ShaderVariable &v, uint32_t row, VarType type)
{
  if(type == VarType::Unknown)
    type = v.type;

  if(v.type == VarType::GPUPointer)
    return ToQStr(v.GetPointer());

  if(v.type == VarType::Struct)
    return lit("{ ... }");

  switch(type)
  {
    case VarType::Float:
      return RowValuesToString((int)v.columns, v.flags, v.value.f32v[row * v.columns + 0],
                               v.value.f32v[row * v.columns + 1], v.value.f32v[row * v.columns + 2],
                               v.value.f32v[row * v.columns + 3]);
    case VarType::Double:
      return RowValuesToString((int)v.columns, v.flags, v.value.f64v[row * v.columns + 0],
                               v.value.f64v[row * v.columns + 1], v.value.f64v[row * v.columns + 2],
                               v.value.f64v[row * v.columns + 3]);
    case VarType::Half:
      return RowValuesToString((int)v.columns, v.flags, v.value.f16v[row * v.columns + 0],
                               v.value.f16v[row * v.columns + 1], v.value.f16v[row * v.columns + 2],
                               v.value.f16v[row * v.columns + 3]);
    case VarType::Bool:
      return RowValuesToString((int)v.columns, v.flags,
                               v.value.u32v[row * v.columns + 0] ? true : false,
                               v.value.u32v[row * v.columns + 1] ? true : false,
                               v.value.u32v[row * v.columns + 2] ? true : false,
                               v.value.u32v[row * v.columns + 3] ? true : false);
    case VarType::ULong:
      return RowValuesToString((int)v.columns, v.flags, v.value.u64v[row * v.columns + 0],
                               v.value.u64v[row * v.columns + 1], v.value.u64v[row * v.columns + 2],
                               v.value.u64v[row * v.columns + 3]);
    case VarType::UInt:
      return RowValuesToString((int)v.columns, v.flags, v.value.u32v[row * v.columns + 0],
                               v.value.u32v[row * v.columns + 1], v.value.u32v[row * v.columns + 2],
                               v.value.u32v[row * v.columns + 3]);
    case VarType::UShort:
      return RowValuesToString((int)v.columns, v.flags, v.value.u16v[row * v.columns + 0],
                               v.value.u16v[row * v.columns + 1], v.value.u16v[row * v.columns + 2],
                               v.value.u16v[row * v.columns + 3]);
    case VarType::UByte:
      return RowValuesToString((int)v.columns, v.flags, v.value.u8v[row * v.columns + 0],
                               v.value.u8v[row * v.columns + 1], v.value.u8v[row * v.columns + 2],
                               v.value.u8v[row * v.columns + 3]);
    case VarType::SLong:
      return RowValuesToString((int)v.columns, v.flags, v.value.s64v[row * v.columns + 0],
                               v.value.s64v[row * v.columns + 1], v.value.s64v[row * v.columns + 2],
                               v.value.s64v[row * v.columns + 3]);
    case VarType::SInt:
      return RowValuesToString((int)v.columns, v.flags, v.value.s32v[row * v.columns + 0],
                               v.value.s32v[row * v.columns + 1], v.value.s32v[row * v.columns + 2],
                               v.value.s32v[row * v.columns + 3]);
    case VarType::SShort:
      return RowValuesToString((int)v.columns, v.flags, v.value.s16v[row * v.columns + 0],
                               v.value.s16v[row * v.columns + 1], v.value.s16v[row * v.columns + 2],
                               v.value.s16v[row * v.columns + 3]);
    case VarType::SByte:
      return RowValuesToString((int)v.columns, v.flags, v.value.s8v[row * v.columns + 0],
                               v.value.s8v[row * v.columns + 1], v.value.s8v[row * v.columns + 2],
                               v.value.s8v[row * v.columns + 3]);
    case VarType::GPUPointer: return ToQStr(v.GetPointer());
    case VarType::Enum:
    case VarType::ConstantBlock:
    case VarType::ReadOnlyResource:
    case VarType::ReadWriteResource:
    case VarType::Sampler:
    case VarType::Unknown:
    case VarType::Struct: break;
  }

  return lit("???");
}

QString VarString(const ShaderVariable &v, const ShaderConstant &c)
{
  if(!v.members.isEmpty())
    return QString();

  if(v.type == VarType::Enum)
  {
    uint64_t val = v.value.u64v[0];

    for(size_t i = 0; i < c.type.members.size(); i++)
      if(val == c.type.members[i].defaultValue)
        return c.type.members[i].name;

    return QApplication::translate("BufferFormatter", "Unknown %1 (%2)")
        .arg(QString(c.type.name))
        .arg(val);
  }

  if(v.rows == 1)
    return RowString(v, 0);

  QString ret;
  for(int i = 0; i < (int)v.rows; i++)
  {
    if(i > 0)
      ret += lit("\n");
    ret += lit("{") + RowString(v, i) + lit("}");
  }

  return ret;
}

QString RowTypeString(const ShaderVariable &v)
{
  if(!v.members.isEmpty() || v.type == VarType::Struct)
  {
    if(v.type == VarType::Struct)
      return lit("struct");
    else
      return lit("flibbertygibbet");
  }

  if(v.rows == 0 && v.columns == 0)
    return lit("-");

  if(v.type == VarType::GPUPointer)
    return PointerTypeRegistry::GetTypeDescriptor(v.GetPointer()).name + "*";

  QString typeStr = ToQStr(v.type);

  if(v.columns == 1)
    return typeStr;

  return QFormatStr("%1%2").arg(typeStr).arg(v.columns);
}

#if ENABLE_UNIT_TESTS

#include "3rdparty/catch/catch.hpp"

TEST_CASE("round-trip via format", "[formatter]")
{
  BufferFormatter::Init(GraphicsAPI::Vulkan);

  ShaderResource res;
  ResourceFormat fmt;
  rdcarray<ShaderConstant> &members = res.variableType.members;
  ParsedFormat parsed;

  members.push_back({});
  members.back().name = "a";
  members.back().byteOffset = 0;
  members.back().type.name = "float";
  members.back().type.flags = ShaderVariableFlags::RowMajorMatrix;
  members.back().type.baseType = VarType::Float;
  members.back().type.arrayByteStride = 16;
  members.back().type.elements = 7;

  // std140 packing
  parsed = BufferFormatter::ParseFormatString(
      BufferFormatter::GetBufferFormatString(
          BufferFormatter::EstimatePackingRules(ResourceId(), members), ResourceId(), res, fmt),
      0, true);

  CHECK((parsed.fixed.type.members == members));

  // std430 packing
  members.back().type.arrayByteStride = 4;

  parsed = BufferFormatter::ParseFormatString(
      BufferFormatter::GetBufferFormatString(
          BufferFormatter::EstimatePackingRules(ResourceId(), members), ResourceId(), res, fmt),
      0, true);

  CHECK((parsed.fixed.type.members == members));

  // scalar packing
  members.back().type.name = "float3";
  members.back().type.columns = 3;
  members.back().type.arrayByteStride = 12;

  parsed = BufferFormatter::ParseFormatString(
      BufferFormatter::GetBufferFormatString(
          BufferFormatter::EstimatePackingRules(ResourceId(), members), ResourceId(), res, fmt),
      0, true);

  CHECK((parsed.fixed.type.members == members));
}

TEST_CASE("Packing rule estimation", "[formatter]")
{
  BufferFormatter::Init(GraphicsAPI::Vulkan);

  // test that each possible "rule violation" of a stricter packing ruleset bumps it down to the
  // appropriate new ruleset, using vulkan for the biggest range.
  ShaderResource res;
  rdcarray<ShaderConstant> &members = res.variableType.members;

  {
    // use a string parse to quickly initialise our template that we'll then fiddle with
    QString tmpFormat = lit(R"(
#pack(std140)

float a;
float2 a;
float3 a;
float4 a;

float a[4];
float2 a[4];
float3 a[4];
float4 a[4];

[[col_major]] float2x4 a;
[[col_major]] float4x2 a;

[[row_major]] float2x4 a;
[[row_major]] float4x2 a;

[[col_major]] float3x4 a;
[[col_major]] float4x3 a;

[[row_major]] float3x4 a;
[[row_major]] float4x3 a;

[[col_major]] float3x4 a[4];
[[col_major]] float4x3 a[4];

[[row_major]] float3x4 a[4];
[[row_major]] float4x3 a[4];

struct struct_with_trailing
{
  float4 inner;
  float4 inner;
  float4 inner;
  float3 inner;
};

struct_with_trailing a;
float trail_test;

float3 a[4];
float trail_test;

float3x4 a;
float trail_test;

)");
    ParsedFormat tmp = BufferFormatter::ParseFormatString(tmpFormat, 1024 * 1024, true);

    members.swap(tmp.fixed.type.members);

    // space everything very conservatively to ensure we don't have to worry about overlaps
    uint32_t byteOffset = 0;
    for(size_t i = 0; i < members.size(); i++)
    {
      members[i].byteOffset = byteOffset;
      byteOffset += 1024;

      if(members[i].name == "trail_test")
        members[i].byteOffset = members[i - 1].byteOffset + 64;
    }
  }

  ResourceFormat fmt;
  ParsedFormat parsed;

  Packing::Rules pack;

  SECTION("No changes")
  {
    // we generated the members with std140 packing so it should stay std140
    pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
    CHECK((pack == Packing::std140));
  }

  SECTION("std140 compatible offsets")
  {
    for(size_t i = 0; i < members.size(); i++)
    {
      if(i == 0)    // float
        members[i].byteOffset += 4;
      else if(i == 1)    // float2
        members[i].byteOffset += 8;
      else
        members[i].byteOffset += 16;
    }

    pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
    CHECK((pack == Packing::std140));
  }

  // no other changes we can make that are std140 compatible, alignments and strides are already at
  // their most conservative

  SECTION("std430 compatible arrays")
  {
    // check that each individual change is detected as std430, to prevent one change from hiding
    // the lack of detection of others
    SECTION("float[4] tight array")
    {
      members[4].type.arrayByteStride = 4;
      pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
      CHECK((pack == Packing::std430));
    }
    SECTION("float2[4] tight array")
    {
      members[5].type.arrayByteStride = 8;
      pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
      CHECK((pack == Packing::std430));
    }
    SECTION("[[col_major]] float2x4 tight array")
    {
      members[8].type.matrixByteStride = 8;
      pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
      CHECK((pack == Packing::std430));
    }
    SECTION("[[row_major]] float4x2 tight array")
    {
      members[11].type.matrixByteStride = 8;
      pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
      CHECK((pack == Packing::std430));
    }
  }

  // D3DCB, on vulkan relaxed block layout, allows 'misaligned' vectors as long as they don't
  // straddle a 16-byte boundary
  SECTION("D3DCB offsets")
  {
    SECTION("float2 4-byte offset")
    {
      members[1].byteOffset += 4;
      pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
      CHECK((pack == Packing::D3DCB));
    }
    SECTION("float3 4-byte offset")
    {
      members[2].byteOffset += 4;
      pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
      CHECK((pack == Packing::D3DCB));
    }
  }

  // it becomes a scalar offset once it straddles
  SECTION("scalar offsets")
  {
    SECTION("float2 12-byte offset")
    {
      members[1].byteOffset += 12;
      pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
      CHECK((pack == Packing::Scalar));
    }
    SECTION("float3 8-byte offset")
    {
      members[2].byteOffset += 8;
      pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
      CHECK((pack == Packing::Scalar));
    }
  }

  SECTION("scalar array strides")
  {
    // float3[4] is the only stride of a pure array that actually changes
    members[6].type.arrayByteStride = 12;
    pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
    CHECK((pack == Packing::Scalar));
  }

  SECTION("scalar matrix strides")
  {
    SECTION("[[col_major]] float3x4 tight matrix")
    {
      members[12].type.matrixByteStride = 12;
      pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
      CHECK((pack == Packing::Scalar));
    }
    SECTION("[[row_major]] float4x3 tight matrix")
    {
      members[15].type.matrixByteStride = 12;
      pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
      CHECK((pack == Packing::Scalar));
    }
  }

  SECTION("trailing struct overlap")
  {
    members[21].byteOffset = members[20].byteOffset + 64 - 4;
    pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
    CHECK((pack == Packing::Scalar));
  }

  SECTION("trailing array overlap")
  {
    members[23].byteOffset = members[22].byteOffset + 64 - 4;
    pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
    CHECK((pack == Packing::Scalar));
  }

  SECTION("trailing matrix overlap")
  {
    members[25].byteOffset = members[24].byteOffset + 64 - 4;
    pack = BufferFormatter::EstimatePackingRules(ResourceId(), members);
    CHECK((pack == Packing::Scalar));
  }
}

TEST_CASE("Buffer format parsing", "[formatter]")
{
  ShaderConstantType float_type;
  float_type.name = "float";
  float_type.flags = ShaderVariableFlags::RowMajorMatrix;
  float_type.baseType = VarType::Float;
  float_type.arrayByteStride = 4;

  ShaderConstantType int_type;
  int_type.name = "int";
  int_type.flags = ShaderVariableFlags::RowMajorMatrix;
  int_type.baseType = VarType::SInt;
  int_type.arrayByteStride = 4;

  ShaderConstantType uint_type;
  uint_type.name = "uint";
  uint_type.flags = ShaderVariableFlags::RowMajorMatrix;
  uint_type.baseType = VarType::UInt;
  uint_type.arrayByteStride = 4;

  ParsedFormat parsed;

  BufferFormatter::Init(GraphicsAPI::Vulkan);

  SECTION("comment, newline, semi-colon and whitespace handling")
  {
    rdcarray<ShaderConstant> members;
    members.push_back({});
    members.back().name = "a";
    members.back().byteOffset = 0;
    members.back().type = float_type;
    members.push_back({});
    members.back().name = "b";
    members.back().byteOffset = 4;
    members.back().type = int_type;
    members.push_back({});
    members.back().name = "c";
    members.back().byteOffset = 8;
    members.back().type = uint_type;

    rdcarray<rdcstr> tests = {
        "float a;\n"
        "int b;\n"
        "uint c;\n",

        "float        a;\n"
        "int   \t \t  b   ;\n"
        "uint   \n  c;\n",

        "float a;int b;uint c;",

        "float a;int b  ;   uint c;",

        "// comment\n"
        "float a;\n"
        "int b;\n"
        "uint c;\n",

        "// commented declaration int x;\n"
        "float a;\n"
        "int b;\n"
        "uint c;\n",

        "/* comment */\n"
        "float a;\n"
        "int b;\n"
        "uint c;\n",

        "/* comment \n"
        " over multiple \n"
        " lines \n"
        "*/\n"
        "float a;\n"
        "int b;\n"
        "uint c;\n",

        "/* commented declaration int x; */\n"
        "float a;\n"
        "int b;\n"
        "uint c;\n",

        "/* comment \n"
        " // nested */ float a;"
        "int b;\n"
        "uint c;\n",

        "/* comment \n"
        " /* with /* extra /* opens */ float a;"
        "int b;\n"
        "uint c;\n",

        "float /* comment in decl */ a /* comment 2 */;\n"
        "int // comment in decl \n"
        "b // comment\n"
        ";\n"
        "uint c;\n",
    };

    for(const rdcstr &test : tests)
    {
      parsed = BufferFormatter::ParseFormatString(test, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      CHECK((parsed.fixed.type.members == members));
    }
  }

  SECTION("blank format")
  {
    ShaderConstantType expected_type = uint_type;
    expected_type.name = "uint4";
    expected_type.flags = ShaderVariableFlags::RowMajorMatrix | ShaderVariableFlags::HexDisplay;
    expected_type.columns = 4;
    expected_type.arrayByteStride = 16;

    parsed = BufferFormatter::ParseFormatString(QString(), 0, false);

    CHECK(parsed.errors.isEmpty());
    CHECK((parsed.repeating.type == expected_type));
  };

  SECTION("Basic formats")
  {
    parsed = BufferFormatter::ParseFormatString(lit("float a;"), 0, true);

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == float_type));

    parsed = BufferFormatter::ParseFormatString(lit("int a;"), 0, true);

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == int_type));

    parsed = BufferFormatter::ParseFormatString(lit("uint a;"), 0, true);

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == uint_type));

    rdcarray<rdcpair<rdcstr, VarType>> tests = {
        {"half", VarType::Half},   {"double", VarType::Double}, {"ubyte", VarType::UByte},
        {"byte", VarType::SByte},  {"ushort", VarType::UShort}, {"short", VarType::SShort},
        {"ulong", VarType::ULong}, {"long", VarType::SLong},    {"bool", VarType::Bool},
    };

    for(const rdcpair<rdcstr, VarType> &test : tests)
    {
      ShaderConstantType expect_type;
      expect_type.name = test.first;
      expect_type.flags = ShaderVariableFlags::RowMajorMatrix;
      expect_type.baseType = test.second;
      expect_type.arrayByteStride = VarTypeByteSize(test.second);

      parsed = BufferFormatter::ParseFormatString(lit("%1 a;").arg(test.first), 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expect_type));
    }

    {
      ShaderConstantType expect_type;
      expect_type.name = "byte";
      expect_type.flags = ShaderVariableFlags::RowMajorMatrix;
      expect_type.baseType = VarType::SByte;
      expect_type.arrayByteStride = 1;

      parsed = BufferFormatter::ParseFormatString(lit("char a;"), 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expect_type));

      expect_type.name = "ubyte";
      expect_type.baseType = VarType::UByte;

      parsed = BufferFormatter::ParseFormatString(lit("unsigned char a;"), 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expect_type));
    }
  };

  SECTION("C-style sized formats")
  {
    parsed = BufferFormatter::ParseFormatString(lit("float32_t a;"), 0, true);

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == float_type));

    parsed = BufferFormatter::ParseFormatString(lit("int32_t a;"), 0, true);

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == int_type));

    parsed = BufferFormatter::ParseFormatString(lit("uint32_t a;"), 0, true);

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == uint_type));

    rdcarray<rdcpair<rdcstr, VarType>> tests = {
        {"float16_t", VarType::Half}, {"float64_t", VarType::Double}, {"uint8_t", VarType::UByte},
        {"int8_t", VarType::SByte},   {"uint16_t", VarType::UShort},  {"int16_t", VarType::SShort},
        {"uint64_t", VarType::ULong}, {"int64_t", VarType::SLong},
    };

    for(uint32_t vecSize = 0; vecSize <= 4; vecSize++)
    {
      for(const rdcpair<rdcstr, VarType> &test : tests)
      {
        ShaderConstantType expect_type;
        expect_type.flags = ShaderVariableFlags::RowMajorMatrix;
        expect_type.baseType = test.second;
        expect_type.columns = qMax(1U, vecSize);
        expect_type.arrayByteStride = VarTypeByteSize(test.second) * expect_type.columns;
        expect_type.name = ToStr(test.second);

        QString format;

        if(vecSize == 0)
        {
          format = lit("%1 a;").arg(test.first);
        }
        else
        {
          format = lit("%1%2 a;").arg(test.first).arg(vecSize);
          expect_type.name += ToStr(vecSize);
        }

        parsed = BufferFormatter::ParseFormatString(format, 0, true);

        CHECK(parsed.errors.isEmpty());
        CHECK(parsed.repeating.type.members.empty());
        REQUIRE(parsed.fixed.type.members.size() == 1);
        CHECK(parsed.fixed.type.members[0].name == "a");
        CHECK((parsed.fixed.type.members[0].type == expect_type));
      }
    }

    {
      ShaderConstantType expect_type;
      expect_type.name = "byte";
      expect_type.flags = ShaderVariableFlags::RowMajorMatrix;
      expect_type.baseType = VarType::SByte;
      expect_type.arrayByteStride = 1;

      parsed = BufferFormatter::ParseFormatString(lit("char a;"), 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expect_type));

      expect_type.name = "ubyte";
      expect_type.baseType = VarType::UByte;

      parsed = BufferFormatter::ParseFormatString(lit("unsigned char a;"), 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expect_type));
    }
  };

  SECTION("Identifier names")
  {
    parsed = BufferFormatter::ParseFormatString(lit("int abcdef;"), 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "abcdef");

    parsed = BufferFormatter::ParseFormatString(lit("int abc_def;"), 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "abc_def");

    parsed = BufferFormatter::ParseFormatString(lit("int __abcdef;"), 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "__abcdef");

    rdcstr longname =
        R"(abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz)";

    parsed = BufferFormatter::ParseFormatString(lit("int %1;").arg(longname), 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == longname);
  };

  SECTION("Vector and matrix types")
  {
    ShaderConstantType expected_type;

    parsed = BufferFormatter::ParseFormatString(lit("float2 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float2";
    expected_type.flags |= ShaderVariableFlags::RowMajorMatrix;
    expected_type.columns = 2;
    expected_type.arrayByteStride = 8;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("float3 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float3";
    expected_type.flags |= ShaderVariableFlags::RowMajorMatrix;
    expected_type.columns = 3;
    expected_type.arrayByteStride = 12;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("float4 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float4";
    expected_type.flags |= ShaderVariableFlags::RowMajorMatrix;
    expected_type.columns = 4;
    expected_type.arrayByteStride = 16;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("float3x2 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float3x2";
    expected_type.flags = ShaderVariableFlags::NoFlags;
    expected_type.rows = 3;
    expected_type.columns = 2;
    // these are calculated with scalar packing for now, packing rules are tested separately
    expected_type.arrayByteStride = 24;
    expected_type.matrixByteStride = 12;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("float2x3 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float2x3";
    expected_type.flags = ShaderVariableFlags::NoFlags;
    expected_type.rows = 2;
    expected_type.columns = 3;
    expected_type.arrayByteStride = 24;
    expected_type.matrixByteStride = 8;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));
  };

  SECTION("GL style vector and matrix types")
  {
    ShaderConstantType expected_type;

    parsed = BufferFormatter::ParseFormatString(lit("vec2 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float2";
    expected_type.flags |= ShaderVariableFlags::RowMajorMatrix;
    expected_type.columns = 2;
    expected_type.arrayByteStride = 8;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("vec3 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float3";
    expected_type.flags |= ShaderVariableFlags::RowMajorMatrix;
    expected_type.columns = 3;
    expected_type.arrayByteStride = 12;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("vec4 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float4";
    expected_type.flags |= ShaderVariableFlags::RowMajorMatrix;
    expected_type.columns = 4;
    expected_type.arrayByteStride = 16;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("mat3x2 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float2x3";
    expected_type.flags = ShaderVariableFlags::NoFlags;
    expected_type.rows = 2;
    expected_type.columns = 3;
    // these are calculated with scalar packing for now, packing rules are tested separately
    expected_type.arrayByteStride = 24;
    expected_type.matrixByteStride = 8;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("mat2x3 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float3x2";
    expected_type.flags = ShaderVariableFlags::NoFlags;
    expected_type.rows = 3;
    expected_type.columns = 2;
    expected_type.arrayByteStride = 24;
    expected_type.matrixByteStride = 12;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("mat3 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float3x3";
    expected_type.flags = ShaderVariableFlags::NoFlags;
    expected_type.rows = 3;
    expected_type.columns = 3;
    // these are calculated with scalar packing for now, packing rules are tested separately
    expected_type.arrayByteStride = 36;
    expected_type.matrixByteStride = 12;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));
  };

  SECTION("Arrays (bounded and unbounded)")
  {
    ShaderConstantType expected_type;

    parsed = BufferFormatter::ParseFormatString(lit("float a[4];"), 0, true);

    expected_type = float_type;
    expected_type.name = "float";
    expected_type.flags = ShaderVariableFlags::RowMajorMatrix;
    expected_type.rows = 1;
    expected_type.columns = 1;
    expected_type.elements = 4;
    expected_type.arrayByteStride = 4;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("float a[];"), 0, true);

    expected_type = float_type;
    expected_type.name = "float";
    expected_type.flags = ShaderVariableFlags::RowMajorMatrix;
    expected_type.rows = 1;
    expected_type.columns = 1;
    expected_type.elements = 1;
    expected_type.arrayByteStride = 4;

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.fixed.type.members.empty());
    CHECK((parsed.repeating.type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("float2 a[4];"), 0, true);

    expected_type = float_type;
    expected_type.name = "float2";
    expected_type.flags = ShaderVariableFlags::RowMajorMatrix;
    expected_type.rows = 1;
    expected_type.columns = 2;
    expected_type.elements = 4;
    expected_type.arrayByteStride = 8;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("float2 a[];"), 0, true);

    expected_type = float_type;
    expected_type.name = "float2";
    expected_type.flags = ShaderVariableFlags::RowMajorMatrix;
    expected_type.rows = 1;
    expected_type.columns = 2;
    expected_type.elements = 1;
    expected_type.arrayByteStride = 8;

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.fixed.type.members.empty());
    CHECK((parsed.repeating.type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("float2x3 a[4];"), 0, true);

    expected_type = float_type;
    expected_type.name = "float2x3";
    expected_type.flags = ShaderVariableFlags::NoFlags;
    expected_type.rows = 2;
    expected_type.columns = 3;
    expected_type.elements = 4;
    expected_type.arrayByteStride = 24;
    expected_type.matrixByteStride = 8;

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("float2x3 a[];"), 0, true);

    expected_type = float_type;
    expected_type.name = "float2x3";
    expected_type.flags = ShaderVariableFlags::NoFlags;
    expected_type.rows = 2;
    expected_type.columns = 3;
    expected_type.elements = 1;
    expected_type.arrayByteStride = 24;
    expected_type.matrixByteStride = 8;

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.fixed.type.members.empty());
    CHECK((parsed.repeating.type == expected_type));
  };

  SECTION("Legacy prefix keywords")
  {
    ShaderConstantType expected_type;

    parsed = BufferFormatter::ParseFormatString(lit("rgb float4 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float4";
    expected_type.flags |= ShaderVariableFlags::RGBDisplay;
    expected_type.columns = 4;
    expected_type.arrayByteStride = 16;

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("row_major float4x4 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float4x4";
    expected_type.flags |= ShaderVariableFlags::RowMajorMatrix;
    expected_type.rows = 4;
    expected_type.columns = 4;
    expected_type.arrayByteStride = 64;
    expected_type.matrixByteStride = 16;

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("column_major float4x4 a;"), 0, true);

    expected_type = float_type;
    expected_type.name = "float4x4";
    expected_type.flags &= ~ShaderVariableFlags::RowMajorMatrix;
    expected_type.rows = 4;
    expected_type.columns = 4;
    expected_type.arrayByteStride = 64;
    expected_type.matrixByteStride = 16;

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == expected_type));
  };

  SECTION("unsigned separate specifier")
  {
    parsed = BufferFormatter::ParseFormatString(lit("unsigned int a;"), 0, true);

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == uint_type));

    parsed = BufferFormatter::ParseFormatString(lit("unsigned short a;"), 0, true);

    ShaderConstantType expected_type;
    expected_type.name = "ushort";
    expected_type.flags = ShaderVariableFlags::RowMajorMatrix;
    expected_type.baseType = VarType::UShort;
    expected_type.arrayByteStride = 2;

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == expected_type));

    parsed = BufferFormatter::ParseFormatString(lit("unsigned byte a;"), 0, true);

    expected_type.name = "ubyte";
    expected_type.baseType = VarType::UByte;
    expected_type.arrayByteStride = 1;

    CHECK(parsed.errors.isEmpty());
    CHECK(parsed.repeating.type.members.empty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "a");
    CHECK((parsed.fixed.type.members[0].type == expected_type));
  };

  SECTION("Legacy base types")
  {
    SECTION("hex")
    {
      parsed = BufferFormatter::ParseFormatString(lit("xint a;"), 0, true);

      ShaderConstantType expected_type = uint_type;
      expected_type.flags |= ShaderVariableFlags::HexDisplay;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("xshort a;"), 0, true);

      expected_type.name = "ushort";
      expected_type.baseType = VarType::UShort;
      expected_type.arrayByteStride = 2;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("xbyte a;"), 0, true);

      expected_type.name = "ubyte";
      expected_type.baseType = VarType::UByte;
      expected_type.arrayByteStride = 1;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("xlong a;"), 0, true);

      expected_type.name = "ulong";
      expected_type.baseType = VarType::ULong;
      expected_type.arrayByteStride = 8;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));
    };

    SECTION("unorm")
    {
      parsed = BufferFormatter::ParseFormatString(lit("unormh a;"), 0, true);

      ShaderConstantType expected_type;
      expected_type.flags = ShaderVariableFlags::RowMajorMatrix | ShaderVariableFlags::UNorm;
      expected_type.name = "ushort";
      expected_type.baseType = VarType::UShort;
      expected_type.arrayByteStride = 2;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("unormb a;"), 0, true);

      expected_type.name = "ubyte";
      expected_type.baseType = VarType::UByte;
      expected_type.arrayByteStride = 1;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));
    };

    SECTION("snorm")
    {
      parsed = BufferFormatter::ParseFormatString(lit("snormh a;"), 0, true);

      ShaderConstantType expected_type;
      expected_type.flags = ShaderVariableFlags::RowMajorMatrix | ShaderVariableFlags::SNorm;
      expected_type.name = "short";
      expected_type.baseType = VarType::SShort;
      expected_type.arrayByteStride = 2;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("snormb a;"), 0, true);

      expected_type.name = "byte";
      expected_type.baseType = VarType::SByte;
      expected_type.arrayByteStride = 1;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));
    };

    SECTION("10:10:10:2")
    {
      parsed = BufferFormatter::ParseFormatString(lit("uintten a;"), 0, true);

      ShaderConstantType expected_type;
      expected_type.flags = ShaderVariableFlags::RowMajorMatrix | ShaderVariableFlags::R10G10B10A2;
      expected_type.name = "uint";
      expected_type.baseType = VarType::UInt;
      expected_type.arrayByteStride = 4;

      expected_type.columns = 4;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("unormten a;"), 0, true);

      expected_type.flags = ShaderVariableFlags::RowMajorMatrix | ShaderVariableFlags::R10G10B10A2 |
                            ShaderVariableFlags::UNorm;
      expected_type.name = "uint";
      expected_type.baseType = VarType::UInt;
      expected_type.arrayByteStride = 4;

      expected_type.columns = 4;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));
    };

    SECTION("11:11:10")
    {
      parsed = BufferFormatter::ParseFormatString(lit("floateleven a;"), 0, true);

      ShaderConstantType expected_type;
      expected_type.flags = ShaderVariableFlags::RowMajorMatrix | ShaderVariableFlags::R11G11B10;
      expected_type.name = "float";
      expected_type.baseType = VarType::Float;
      expected_type.arrayByteStride = 4;

      expected_type.columns = 3;

      CHECK(parsed.errors.isEmpty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK((parsed.fixed.type.members[0].type == expected_type));
    };
  };

  SECTION("enums")
  {
    rdcstr def = R"(
enum MyEnum : uint
{
  Val1 = 1,
  Val2 = 2,
  ValHex = 0xf,
};

MyEnum e;
)";
    parsed = BufferFormatter::ParseFormatString(def, 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "e");
    CHECK(parsed.fixed.type.members[0].type.name == "MyEnum");
    CHECK(parsed.fixed.type.members[0].type.baseType == VarType::Enum);
    CHECK(parsed.fixed.type.members[0].type.matrixByteStride == 4);
    CHECK(parsed.fixed.type.members[0].type.members[0].name == "Val1");
    CHECK(parsed.fixed.type.members[0].type.members[0].defaultValue == 1);
    CHECK(parsed.fixed.type.members[0].type.members[1].name == "Val2");
    CHECK(parsed.fixed.type.members[0].type.members[1].defaultValue == 2);
    CHECK(parsed.fixed.type.members[0].type.members[2].name == "ValHex");
    CHECK(parsed.fixed.type.members[0].type.members[2].defaultValue == 0xf);

    def = R"(
enum MyEnum : ushort
{
  Val = 0,
};

MyEnum e;
)";
    parsed = BufferFormatter::ParseFormatString(def, 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK(parsed.fixed.type.members[0].name == "e");
    CHECK(parsed.fixed.type.members[0].type.name == "MyEnum");
    CHECK(parsed.fixed.type.members[0].type.baseType == VarType::Enum);
    CHECK(parsed.fixed.type.members[0].type.matrixByteStride == 2);
    CHECK(parsed.fixed.type.members[0].type.members[0].name == "Val");
    CHECK(parsed.fixed.type.members[0].type.members[0].defaultValue == 0);
  };

  SECTION("bitfields")
  {
    rdcstr def = R"(
uint firstbyte : 8;
uint halfsecondbyte : 4;
uint halfsecondbyte2 : 4;
uint lasttwo : 16;
)";
    parsed = BufferFormatter::ParseFormatString(def, 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 4);
    CHECK(parsed.fixed.type.arrayByteStride == 4);
    CHECK(parsed.fixed.type.members[0].name == "firstbyte");
    CHECK(parsed.fixed.type.members[0].bitFieldOffset == 0);
    CHECK(parsed.fixed.type.members[0].bitFieldSize == 8);
    CHECK((parsed.fixed.type.members[0].type == uint_type));
    CHECK(parsed.fixed.type.members[1].name == "halfsecondbyte");
    CHECK(parsed.fixed.type.members[1].bitFieldOffset == 8);
    CHECK(parsed.fixed.type.members[1].bitFieldSize == 4);
    CHECK((parsed.fixed.type.members[1].type == uint_type));
    CHECK(parsed.fixed.type.members[2].name == "halfsecondbyte2");
    CHECK(parsed.fixed.type.members[2].bitFieldOffset == 12);
    CHECK(parsed.fixed.type.members[2].bitFieldSize == 4);
    CHECK((parsed.fixed.type.members[2].type == uint_type));
    CHECK(parsed.fixed.type.members[3].name == "lasttwo");
    CHECK(parsed.fixed.type.members[3].bitFieldOffset == 16);
    CHECK(parsed.fixed.type.members[3].bitFieldSize == 16);
    CHECK((parsed.fixed.type.members[3].type == uint_type));

    def = R"(
enum e : ushort { val = 5 };

uint firstbyte : 8;
uint halfsecondbyte : 4;
uint halfsecondbyte2 : 4;
e lastenum : 16;
)";
    parsed = BufferFormatter::ParseFormatString(def, 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 4);
    CHECK(parsed.fixed.type.arrayByteStride == 4);
    CHECK(parsed.fixed.type.members[0].name == "firstbyte");
    CHECK(parsed.fixed.type.members[0].bitFieldOffset == 0);
    CHECK(parsed.fixed.type.members[0].bitFieldSize == 8);
    CHECK((parsed.fixed.type.members[0].type == uint_type));
    CHECK(parsed.fixed.type.members[1].name == "halfsecondbyte");
    CHECK(parsed.fixed.type.members[1].bitFieldOffset == 8);
    CHECK(parsed.fixed.type.members[1].bitFieldSize == 4);
    CHECK((parsed.fixed.type.members[1].type == uint_type));
    CHECK(parsed.fixed.type.members[2].name == "halfsecondbyte2");
    CHECK(parsed.fixed.type.members[2].bitFieldOffset == 12);
    CHECK(parsed.fixed.type.members[2].bitFieldSize == 4);
    CHECK((parsed.fixed.type.members[2].type == uint_type));
    CHECK(parsed.fixed.type.members[3].name == "lastenum");
    CHECK(parsed.fixed.type.members[3].byteOffset == 2);
    CHECK(parsed.fixed.type.members[3].bitFieldOffset == 0);
    CHECK(parsed.fixed.type.members[3].bitFieldSize == 16);
    CHECK(parsed.fixed.type.members[3].type.baseType == VarType::Enum);

    def = R"(
uint firstbyte : 8;
uint : 4;
uint halfsecondbyte2 : 4;
uint lasttwo : 16;
)";
    parsed = BufferFormatter::ParseFormatString(def, 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 3);
    CHECK(parsed.fixed.type.arrayByteStride == 4);
    CHECK(parsed.fixed.type.members[0].name == "firstbyte");
    CHECK(parsed.fixed.type.members[0].bitFieldOffset == 0);
    CHECK(parsed.fixed.type.members[0].bitFieldSize == 8);
    CHECK((parsed.fixed.type.members[0].type == uint_type));
    // bits skipped
    CHECK(parsed.fixed.type.members[1].name == "halfsecondbyte2");
    CHECK(parsed.fixed.type.members[1].bitFieldOffset == 12);
    CHECK(parsed.fixed.type.members[1].bitFieldSize == 4);
    CHECK((parsed.fixed.type.members[1].type == uint_type));
    CHECK(parsed.fixed.type.members[2].name == "lasttwo");
    CHECK(parsed.fixed.type.members[2].bitFieldOffset == 16);
    CHECK(parsed.fixed.type.members[2].bitFieldSize == 16);
    CHECK((parsed.fixed.type.members[2].type == uint_type));

    def = R"(
unsigned char bit0 : 1;
unsigned char bit1 : 1;
unsigned char bit2 : 1;
unsigned char bit3 : 1;
unsigned char : 3;
unsigned char highbit : 1;
)";
    parsed = BufferFormatter::ParseFormatString(def, 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 5);
    CHECK(parsed.fixed.type.arrayByteStride == 1);
    CHECK(parsed.fixed.type.members[0].name == "bit0");
    CHECK(parsed.fixed.type.members[0].bitFieldOffset == 0);
    CHECK(parsed.fixed.type.members[0].bitFieldSize == 1);
    CHECK(parsed.fixed.type.members[1].name == "bit1");
    CHECK(parsed.fixed.type.members[1].bitFieldOffset == 1);
    CHECK(parsed.fixed.type.members[1].bitFieldSize == 1);
    CHECK(parsed.fixed.type.members[2].name == "bit2");
    CHECK(parsed.fixed.type.members[2].bitFieldOffset == 2);
    CHECK(parsed.fixed.type.members[2].bitFieldSize == 1);
    CHECK(parsed.fixed.type.members[3].name == "bit3");
    CHECK(parsed.fixed.type.members[3].bitFieldOffset == 3);
    CHECK(parsed.fixed.type.members[3].bitFieldSize == 1);
    CHECK(parsed.fixed.type.members[4].name == "highbit");
    CHECK(parsed.fixed.type.members[4].bitFieldOffset == 7);
    CHECK(parsed.fixed.type.members[4].bitFieldSize == 1);

    def = R"(
uint infirst : 16;
uint infirstalso : 14;
// this will be forced to the second uint, leaving 2 bits of trailing padding in the first
uint overflow : 20;
// this then also can't be packed into the second uint and will be packed into a third, leaving 12 bits of padding
uint inthird : 14;

// result: total of 64 bits but packed into 3 uints due to padding
)";
    parsed = BufferFormatter::ParseFormatString(def, 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 4);
    CHECK(parsed.fixed.type.arrayByteStride == 12);
    CHECK(parsed.fixed.type.members[0].name == "infirst");
    CHECK(parsed.fixed.type.members[0].byteOffset == 0);
    CHECK(parsed.fixed.type.members[0].bitFieldOffset == 0);
    CHECK(parsed.fixed.type.members[0].bitFieldSize == 16);
    CHECK(parsed.fixed.type.members[1].name == "infirstalso");
    CHECK(parsed.fixed.type.members[1].byteOffset == 0);
    CHECK(parsed.fixed.type.members[1].bitFieldOffset == 16);
    CHECK(parsed.fixed.type.members[1].bitFieldSize == 14);
    CHECK(parsed.fixed.type.members[2].name == "overflow");
    CHECK(parsed.fixed.type.members[2].byteOffset == 4);
    CHECK(parsed.fixed.type.members[2].bitFieldOffset == 0);
    CHECK(parsed.fixed.type.members[2].bitFieldSize == 20);
    CHECK(parsed.fixed.type.members[3].name == "inthird");
    CHECK(parsed.fixed.type.members[3].byteOffset == 8);
    CHECK(parsed.fixed.type.members[3].bitFieldOffset == 0);
    CHECK(parsed.fixed.type.members[3].bitFieldSize == 14);
  };

  SECTION("pointers")
  {
    rdcstr def = R"(
struct inner
{
  int a;
  float b;
};

inner *ptr;
int count;
)";

    parsed = BufferFormatter::ParseFormatString(def, 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 2);
    CHECK(parsed.fixed.type.members[0].name == "ptr");
    CHECK(parsed.fixed.type.members[0].type.baseType == VarType::GPUPointer);
    CHECK(parsed.fixed.type.members[1].name == "count");
    CHECK((parsed.fixed.type.members[1].type == int_type));

    REQUIRE(parsed.fixed.type.members[0].type.pointerTypeID != ~0U);

    const ShaderConstantType &ptrType =
        PointerTypeRegistry::GetTypeDescriptor(parsed.fixed.type.members[0].type.pointerTypeID);

    REQUIRE(ptrType.members.size() == 2);
    CHECK(ptrType.members[0].name == "a");
    CHECK((ptrType.members[0].type == int_type));
    CHECK(ptrType.members[1].name == "b");
    CHECK((ptrType.members[1].type == float_type));
  };

  SECTION("structs")
  {
    rdcstr def = R"(
struct inner
{
  int a;
  float b;
};

struct outer
{
  inner first;
  float c;
  inner array[3];
  float d;
};
)";
    parsed = BufferFormatter::ParseFormatString(def + "\nouter o;", 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    const ShaderConstant &o = parsed.fixed.type.members[0];

    CHECK(o.name == "o");
    CHECK(o.type.name == "outer");
    CHECK(o.type.baseType == VarType::Struct);
    CHECK(o.type.arrayByteStride == 40);
    REQUIRE(o.type.members.size() == 4);

    CHECK(o.type.members[0].name == "first");
    CHECK(o.type.members[0].byteOffset == 0);
    CHECK(o.type.members[1].name == "c");
    CHECK(o.type.members[1].byteOffset == 8);
    CHECK((o.type.members[1].type == float_type));
    CHECK(o.type.members[2].name == "array");
    CHECK(o.type.members[2].byteOffset == 12);
    CHECK(o.type.members[3].name == "d");
    CHECK(o.type.members[3].byteOffset == 36);
    CHECK((o.type.members[3].type == float_type));

    const ShaderConstant &first = o.type.members[0];
    const ShaderConstant &array = o.type.members[2];

    CHECK((first.type.members == array.type.members));
    REQUIRE(first.type.members.size() == 2);
    CHECK(first.type.members[0].name == "a");
    CHECK(first.type.members[0].byteOffset == 0);
    CHECK((first.type.members[0].type == int_type));
    CHECK(first.type.members[1].name == "b");
    CHECK(first.type.members[1].byteOffset == 4);
    CHECK((first.type.members[1].type == float_type));

    ParsedFormat parsed2 = BufferFormatter::ParseFormatString(def, 0, true);

    CHECK(parsed.errors.isEmpty());
    REQUIRE(parsed.fixed.type.members.size() == 1);
    CHECK((o.type.members == parsed2.fixed.type.members));
  };

  SECTION("annotations")
  {
    ShaderConstantType expected_type;
    rdcstr def;

    SECTION("general annotation parsing")
    {
      parsed = BufferFormatter::ParseFormatString(lit("[[rgb]]\nfloat4 a;"), 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].type.flags & ShaderVariableFlags::RGBDisplay);

      parsed = BufferFormatter::ParseFormatString(lit("[[rgb]] float4 a;"), 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].type.flags & ShaderVariableFlags::RGBDisplay);

      parsed = BufferFormatter::ParseFormatString(lit("[[rgb]]    \n \n    \n  float4 a;"), 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].type.flags & ShaderVariableFlags::RGBDisplay);

      parsed = BufferFormatter::ParseFormatString(
          lit("[[rgb]]    \n // comment \n /* comment */    \n  float4 a;"), 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].type.flags & ShaderVariableFlags::RGBDisplay);
    }

    SECTION("[[rgb]]")
    {
      parsed = BufferFormatter::ParseFormatString(lit("[[rgb]] float4 a;"), 0, true);

      expected_type = float_type;
      expected_type.name = "float4";
      expected_type.flags |= ShaderVariableFlags::RGBDisplay;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));
    };

    SECTION("[[hex]]")
    {
      parsed = BufferFormatter::ParseFormatString(lit("[[hex]] uint4 a;"), 0, true);

      expected_type = uint_type;
      expected_type.name = "uint4";
      expected_type.flags |= ShaderVariableFlags::HexDisplay;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("[[hexadecimal]] uint4 a;"), 0, true);

      expected_type = uint_type;
      expected_type.name = "uint4";
      expected_type.flags |= ShaderVariableFlags::HexDisplay;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));
    };

    SECTION("[[bin]]")
    {
      parsed = BufferFormatter::ParseFormatString(lit("[[bin]] uint4 a;"), 0, true);

      expected_type = uint_type;
      expected_type.name = "uint4";
      expected_type.flags |= ShaderVariableFlags::BinaryDisplay;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("[[binary]] uint4 a;"), 0, true);

      expected_type = uint_type;
      expected_type.name = "uint4";
      expected_type.flags |= ShaderVariableFlags::BinaryDisplay;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));
    };

    SECTION("[[row_major]] and [[col_major]]")
    {
      parsed = BufferFormatter::ParseFormatString(lit("[[row_major]] float4x4 a;"), 0, true);

      expected_type = float_type;
      expected_type.name = "float4x4";
      expected_type.flags |= ShaderVariableFlags::RowMajorMatrix;
      expected_type.rows = 4;
      expected_type.columns = 4;
      expected_type.matrixByteStride = 16;
      expected_type.arrayByteStride = 64;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("[[col_major]] float4x4 a;"), 0, true);

      expected_type = float_type;
      expected_type.name = "float4x4";
      expected_type.flags &= ~ShaderVariableFlags::RowMajorMatrix;
      expected_type.rows = 4;
      expected_type.columns = 4;
      expected_type.matrixByteStride = 16;
      expected_type.arrayByteStride = 64;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));
    };

    SECTION("[[unorm]] and [[snorm]]")
    {
      parsed = BufferFormatter::ParseFormatString(lit("[[unorm]] ushort4 a;"), 0, true);

      expected_type = uint_type;
      expected_type.name = "ushort4";
      expected_type.flags |= ShaderVariableFlags::UNorm;
      expected_type.baseType = VarType::UShort;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 8;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("[[unorm]] ubyte4 a;"), 0, true);

      expected_type = uint_type;
      expected_type.name = "ubyte4";
      expected_type.flags |= ShaderVariableFlags::UNorm;
      expected_type.baseType = VarType::UByte;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 4;

      parsed = BufferFormatter::ParseFormatString(lit("[[snorm]] short4 a;"), 0, true);

      expected_type = int_type;
      expected_type.name = "short4";
      expected_type.flags |= ShaderVariableFlags::SNorm;
      expected_type.baseType = VarType::SShort;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 8;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("[[snorm]] byte4 a;"), 0, true);

      expected_type = int_type;
      expected_type.name = "byte4";
      expected_type.flags |= ShaderVariableFlags::SNorm;
      expected_type.baseType = VarType::SByte;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 4;
    };

    SECTION("[[packed]]")
    {
      parsed = BufferFormatter::ParseFormatString(lit("[[packed(r11g11b10)]] float3 a;"), 0, true);

      expected_type = float_type;
      expected_type.name = "float3";
      expected_type.flags |= ShaderVariableFlags::R11G11B10;
      expected_type.columns = 3;
      expected_type.arrayByteStride = 4;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("[[packed(r10g10b10a2)]] uint4 a;"), 0, true);

      expected_type = uint_type;
      expected_type.name = "uint4";
      expected_type.flags |= ShaderVariableFlags::R10G10B10A2;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 4;

      parsed =
          BufferFormatter::ParseFormatString(lit("[[packed(r10g10b10a2_uint)]] uint4 a;"), 0, true);

      expected_type = uint_type;
      expected_type.name = "uint4";
      expected_type.flags |= ShaderVariableFlags::R10G10B10A2;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 4;

      parsed = BufferFormatter::ParseFormatString(lit("[[unorm]] [[packed(r10g10b10a2)]] uint4 a;"),
                                                  0, true);

      expected_type = uint_type;
      expected_type.name = "uint4";
      expected_type.flags |= ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::UNorm;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 4;

      parsed = BufferFormatter::ParseFormatString(lit("[[snorm]] [[packed(r10g10b10a2)]] uint4 a;"),
                                                  0, true);

      expected_type = uint_type;
      expected_type.name = "uint4";
      expected_type.flags |= ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::SNorm;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 4;

      parsed =
          BufferFormatter::ParseFormatString(lit("[[packed(r10g10b10a2_unorm)]] uint4 a;"), 0, true);

      expected_type = uint_type;
      expected_type.name = "uint4";
      expected_type.flags |= ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::UNorm;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 4;

      parsed =
          BufferFormatter::ParseFormatString(lit("[[packed(r10g10b10a2_snorm)]] uint4 a;"), 0, true);

      expected_type = uint_type;
      expected_type.name = "uint4";
      expected_type.flags |= ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::SNorm;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 4;

      parsed = BufferFormatter::ParseFormatString(lit("[[snorm]] [[packed(r10g10b10a2)]] int4 a;"),
                                                  0, true);

      expected_type = int_type;
      expected_type.name = "int4";
      expected_type.flags |= ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::SNorm;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 4;

      parsed =
          BufferFormatter::ParseFormatString(lit("[[packed(r10g10b10a2_snorm)]] int4 a;"), 0, true);

      expected_type = int_type;
      expected_type.name = "int4";
      expected_type.flags |= ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::SNorm;
      expected_type.columns = 4;
      expected_type.arrayByteStride = 4;
    };

    SECTION("[[single]]")
    {
      parsed = BufferFormatter::ParseFormatString(lit("float4 a;"), 0, false);

      expected_type = float_type;
      expected_type.name = "float4";
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.fixed.type.members.empty());
      CHECK(parsed.repeating.name == "a");
      CHECK((parsed.repeating.type == expected_type));

      parsed = BufferFormatter::ParseFormatString(lit("[[single]] float4 a;"), 0, false);

      expected_type = float_type;
      expected_type.name = "float4";
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      def = R"(
struct Single
{
  float4 a;
};

Single s;
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, false);

      expected_type = float_type;
      expected_type.name = "float4";
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.fixed.type.members.empty());
      CHECK(parsed.repeating.name == "s");
      REQUIRE(parsed.repeating.type.members.size() == 1);
      CHECK(parsed.repeating.type.members[0].name == "a");
      CHECK((parsed.repeating.type.members[0].type == expected_type));

      def = R"(
[[single]]
struct Single
{
  float4 a;
};
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, false);

      expected_type = float_type;
      expected_type.name = "float4";
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      def = R"(
[[fixed]]
struct Single
{
  float4 a;
};
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, false);

      expected_type = float_type;
      expected_type.name = "float4";
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type == expected_type));

      def = R"(
struct Single
{
  float4 a;
};

[[single]]
Single s;
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, false);

      expected_type = float_type;
      expected_type.name = "float4";
      expected_type.columns = 4;
      expected_type.arrayByteStride = 16;

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].name == "s");
      REQUIRE(parsed.fixed.type.members[0].type.members.size() == 1);
      CHECK(parsed.fixed.type.members[0].type.members[0].name == "a");
      CHECK((parsed.fixed.type.members[0].type.members[0].type == expected_type));
    };

    SECTION("[[size]]")
    {
      def = R"(
[[size(128)]]
struct s
{
  float4 a;
};
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.arrayByteStride == 128);

      def = R"(
[[size(128)]]
struct s
{
  float4 a;
};

s value;
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.arrayByteStride == 128);

      def = R"(
[[byte_size(128)]]
struct s
{
  float4 a;
};
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 1);
      CHECK(parsed.fixed.type.arrayByteStride == 128);
    };

    SECTION("[[offset]]")
    {
      def = R"(
struct s
{
  [[offset(16)]]
  float4 a;
  [[offset(128)]]
  float4 b;
};
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 2);
      CHECK(parsed.fixed.type.members[0].byteOffset == 16);
      CHECK(parsed.fixed.type.members[1].byteOffset == 128);
    };

    SECTION("[[pad]]")
    {
      def = R"(
struct s
{
  [[pad]]
  int4 pad;
  float4 a;
  [[padding]]
  int pad[24];
  float4 b;
};
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 2);
      CHECK(parsed.fixed.type.members[0].byteOffset == 16);
      CHECK(parsed.fixed.type.members[1].byteOffset == 128);
    };
  };

  SECTION("packing rules")
  {
    SECTION("Check API defaults")
    {
      BufferFormatter::Init(GraphicsAPI::D3D11);
      parsed = BufferFormatter::ParseFormatString(lit("float a;"), 0, true);
      CHECK((parsed.packing == Packing::D3DCB));

      BufferFormatter::Init(GraphicsAPI::D3D11);
      parsed = BufferFormatter::ParseFormatString(lit("float a;"), 0, false);
      CHECK((parsed.packing == Packing::D3DUAV));

      BufferFormatter::Init(GraphicsAPI::D3D12);
      parsed = BufferFormatter::ParseFormatString(lit("float a;"), 0, true);
      CHECK((parsed.packing == Packing::D3DCB));

      BufferFormatter::Init(GraphicsAPI::D3D12);
      parsed = BufferFormatter::ParseFormatString(lit("float a;"), 0, false);
      CHECK((parsed.packing == Packing::D3DUAV));

      BufferFormatter::Init(GraphicsAPI::OpenGL);
      parsed = BufferFormatter::ParseFormatString(lit("float a;"), 0, true);
      CHECK((parsed.packing == Packing::std140));

      BufferFormatter::Init(GraphicsAPI::OpenGL);
      parsed = BufferFormatter::ParseFormatString(lit("float a;"), 0, false);
      CHECK((parsed.packing == Packing::std430));
    };

    SECTION("Overriding API defaults")
    {
      BufferFormatter::Init(GraphicsAPI::D3D11);
      parsed = BufferFormatter::ParseFormatString(lit("#pack(c)\nfloat a;"), 0, true);
      CHECK((parsed.packing == Packing::C));

      BufferFormatter::Init(GraphicsAPI::D3D11);
      parsed = BufferFormatter::ParseFormatString(lit("#pack(c)\nfloat a;"), 0, false);
      CHECK((parsed.packing == Packing::C));

      BufferFormatter::Init(GraphicsAPI::D3D12);
      parsed = BufferFormatter::ParseFormatString(lit("#pack(c)\nfloat a;"), 0, true);
      CHECK((parsed.packing == Packing::C));

      BufferFormatter::Init(GraphicsAPI::D3D12);
      parsed = BufferFormatter::ParseFormatString(lit("#pack(c)\nfloat a;"), 0, false);
      CHECK((parsed.packing == Packing::C));

      BufferFormatter::Init(GraphicsAPI::OpenGL);
      parsed = BufferFormatter::ParseFormatString(lit("#pack(c)\nfloat a;"), 0, true);
      CHECK((parsed.packing == Packing::C));

      BufferFormatter::Init(GraphicsAPI::OpenGL);
      parsed = BufferFormatter::ParseFormatString(lit("#pack(c)\nfloat a;"), 0, false);
      CHECK((parsed.packing == Packing::C));
    };

    SECTION("Parsing")
    {
      BufferFormatter::Init(GraphicsAPI::OpenGL);
      parsed = BufferFormatter::ParseFormatString(lit("#pack  (c)\nfloat a;"), 0, false);
      CHECK((parsed.packing == Packing::C));

      BufferFormatter::Init(GraphicsAPI::OpenGL);
      parsed = BufferFormatter::ParseFormatString(lit("#  pack  (c)\nfloat a;"), 0, false);
      CHECK((parsed.packing == Packing::C));

      BufferFormatter::Init(GraphicsAPI::OpenGL);
      parsed = BufferFormatter::ParseFormatString(
          lit("# /*comm*/ pack  /* comments */ (c)\nfloat a;"), 0, false);
      CHECK((parsed.packing == Packing::C));
    };

    SECTION("Selecting packing rules")
    {
      // this will produce different offsets on every packing rule
      rdcstr def = R"(
struct inner
{
   float first;
   byte second;
};

struct s
{
  int a;
  float3 b; // if vectors are aligned to components this is tightly packed, otherwise padded


  [[offset(32)]]
  float c;
  float4 d; // if vectors can straddle this is tightly packed, otherwise padded

  [[offset(64)]]
  float e[5];
  float f; // this will be placed differently depending on whether there are tight arrays, and if
           // there aren't tight arrays whether trailing padding can be overlapped

  [[offset(160)]]
  inner g;
  byte h;  // if trailing padding can be overlapped this will be 'inside' g
};
)";
      parsed = BufferFormatter::ParseFormatString(lit("#pack(cbuffer)\n") + def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 8);
      CHECK(parsed.fixed.type.members[0].byteOffset == 0);    // a
      CHECK(parsed.fixed.type.members[1].byteOffset == 4);    // b

      CHECK(parsed.fixed.type.members[2].byteOffset == 32);    // c
      CHECK(parsed.fixed.type.members[3].byteOffset == 48);    // d

      CHECK(parsed.fixed.type.members[4].byteOffset == 64);     // e
      CHECK(parsed.fixed.type.members[5].byteOffset == 132);    // f

      CHECK(parsed.fixed.type.members[6].byteOffset == 160);    // g
      CHECK(parsed.fixed.type.members[7].byteOffset == 165);    // h

      parsed = BufferFormatter::ParseFormatString(lit("#pack(d3duav)\n") + def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 8);
      CHECK(parsed.fixed.type.members[0].byteOffset == 0);    // a
      CHECK(parsed.fixed.type.members[1].byteOffset == 4);    // b

      CHECK(parsed.fixed.type.members[2].byteOffset == 32);    // c
      CHECK(parsed.fixed.type.members[3].byteOffset == 36);    // d

      CHECK(parsed.fixed.type.members[4].byteOffset == 64);    // e
      CHECK(parsed.fixed.type.members[5].byteOffset == 84);    // f

      CHECK(parsed.fixed.type.members[6].byteOffset == 160);    // g
      CHECK(parsed.fixed.type.members[7].byteOffset == 168);    // h

      parsed = BufferFormatter::ParseFormatString(lit("#pack(std140)\n") + def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 8);
      CHECK(parsed.fixed.type.members[0].byteOffset == 0);     // a
      CHECK(parsed.fixed.type.members[1].byteOffset == 16);    // b

      CHECK(parsed.fixed.type.members[2].byteOffset == 32);    // c
      CHECK(parsed.fixed.type.members[3].byteOffset == 48);    // d

      CHECK(parsed.fixed.type.members[4].byteOffset == 64);     // e
      CHECK(parsed.fixed.type.members[5].byteOffset == 144);    // f

      CHECK(parsed.fixed.type.members[6].byteOffset == 160);    // g
      CHECK(parsed.fixed.type.members[7].byteOffset == 176);    // h

      parsed = BufferFormatter::ParseFormatString(lit("#pack(std430)\n") + def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 8);
      CHECK(parsed.fixed.type.members[0].byteOffset == 0);     // a
      CHECK(parsed.fixed.type.members[1].byteOffset == 16);    // b

      CHECK(parsed.fixed.type.members[2].byteOffset == 32);    // c
      CHECK(parsed.fixed.type.members[3].byteOffset == 48);    // d

      CHECK(parsed.fixed.type.members[4].byteOffset == 64);    // e
      CHECK(parsed.fixed.type.members[5].byteOffset == 84);    // f

      CHECK(parsed.fixed.type.members[6].byteOffset == 160);    // g
      CHECK(parsed.fixed.type.members[7].byteOffset == 168);    // h

      parsed = BufferFormatter::ParseFormatString(lit("#pack(scalar)\n") + def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 8);
      CHECK(parsed.fixed.type.members[0].byteOffset == 0);    // a
      CHECK(parsed.fixed.type.members[1].byteOffset == 4);    // b

      CHECK(parsed.fixed.type.members[2].byteOffset == 32);    // c
      CHECK(parsed.fixed.type.members[3].byteOffset == 36);    // d

      CHECK(parsed.fixed.type.members[4].byteOffset == 64);    // e
      CHECK(parsed.fixed.type.members[5].byteOffset == 84);    // f

      CHECK(parsed.fixed.type.members[6].byteOffset == 160);    // g
      CHECK(parsed.fixed.type.members[7].byteOffset == 165);    // h
    };

    SECTION("Additional packing rules")
    {
      rdcstr def = R"(
#pack(tight_bitfield_packing)

uint infirst : 16;
uint infirstalso : 14;
// this will span 2 bits in the first uint and 18 bits in the second
uint overflow : 20;
uint insecond : 14;
)";
      for(rdcstr ruleset :
          {"", "#pack(c)", "#pack(scalar)", "#pack(std430)", "#pack(std140)", "#pack(cbuffer)"})
      {
        parsed = BufferFormatter::ParseFormatString(ruleset + "\n" + def, 0, true);

        CHECK(parsed.errors.isEmpty());
        REQUIRE(parsed.fixed.type.members.size() == 4);
        CHECK(parsed.fixed.type.arrayByteStride == 8);
        CHECK(parsed.fixed.type.members[0].name == "infirst");
        CHECK(parsed.fixed.type.members[0].byteOffset == 0);
        CHECK(parsed.fixed.type.members[0].bitFieldOffset == 0);
        CHECK(parsed.fixed.type.members[0].bitFieldSize == 16);
        CHECK(parsed.fixed.type.members[1].name == "infirstalso");
        CHECK(parsed.fixed.type.members[1].byteOffset == 0);
        CHECK(parsed.fixed.type.members[1].bitFieldOffset == 16);
        CHECK(parsed.fixed.type.members[1].bitFieldSize == 14);
        CHECK(parsed.fixed.type.members[2].name == "overflow");
        CHECK(parsed.fixed.type.members[2].byteOffset == 3);
        CHECK(parsed.fixed.type.members[2].bitFieldOffset == 6);
        CHECK(parsed.fixed.type.members[2].bitFieldSize == 20);
        CHECK(parsed.fixed.type.members[3].name == "insecond");
        CHECK(parsed.fixed.type.members[3].byteOffset == 6);
        CHECK(parsed.fixed.type.members[3].bitFieldOffset == 2);
        CHECK(parsed.fixed.type.members[3].bitFieldSize == 14);
      }
    };

    SECTION("Testing trailing member alignments")
    {
      rdcstr def = R"(
#pack(std140)

struct blah
{
   float4x4 a;
   float4 b;
};
)";
      parsed = BufferFormatter::ParseFormatString(def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 2);
      CHECK(parsed.fixed.type.members[0].byteOffset == 0);     // a
      CHECK(parsed.fixed.type.members[1].byteOffset == 64);    // b
      CHECK(parsed.fixed.type.arrayByteStride == 80);          // stride 80

      def = R"(
#pack(std140)

struct blah
{
   float4x4 a;
   uint b;
   uint c;
   uint d;
   uint e;
};
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 5);
      CHECK(parsed.fixed.type.members[0].byteOffset == 0);     // a
      CHECK(parsed.fixed.type.members[1].byteOffset == 64);    // b
      CHECK(parsed.fixed.type.members[2].byteOffset == 68);    // c
      CHECK(parsed.fixed.type.members[3].byteOffset == 72);    // d
      CHECK(parsed.fixed.type.members[4].byteOffset == 76);    // e
      CHECK(parsed.fixed.type.arrayByteStride == 80);          // stride 80

      def = R"(
#pack(std140)

struct blah
{
   float4x4 a;
   uint b;
   uint c;
   float d;
   uint e;
};
)";

      parsed = BufferFormatter::ParseFormatString(def, 0, true);

      CHECK(parsed.errors.isEmpty());
      CHECK(parsed.repeating.type.members.empty());
      REQUIRE(parsed.fixed.type.members.size() == 5);
      CHECK(parsed.fixed.type.members[0].byteOffset == 0);     // a
      CHECK(parsed.fixed.type.members[1].byteOffset == 64);    // b
      CHECK(parsed.fixed.type.members[2].byteOffset == 68);    // c
      CHECK(parsed.fixed.type.members[3].byteOffset == 72);    // d
      CHECK(parsed.fixed.type.members[4].byteOffset == 76);    // e
      CHECK(parsed.fixed.type.arrayByteStride == 80);          // stride 80
    };
  };

  SECTION("errors")
  {
    rdcstr def;

    // we don't check exact error text, we check that an error is found and on the right line, and
    // contains a keyword indicating that the error is the right place. This avoids needing to
    // update the test every time the error text changes.
    // note line numbers are 0-based

    struct error_expect
    {
      rdcstr text;
      int line;
      rdcstr error;
    };

    rdcarray<error_expect> errors;

    SECTION("line numbers are accurate around whitespace and comments")
    {
      errors = {
          {"flibble boo;", 0, "parse declaration"},
          {R"(
flibble boo;
)",
           1, "parse declaration"},

          {R"(



flibble boo;
)",
           4, "parse declaration"},
          {R"(
/*
comments
*/
flibble boo;
)",
           4, "parse declaration"},
          {R"(
//
// comments
//
flibble boo;
)",
           4, "parse declaration"},
          {R"(
//
// comments
//
flibble /*
*/boo;
)",
           5, "parse declaration"},

          {R"(
//
// comments
//
flibble /*
*/

boo;
)",
           7, "parse declaration"},
      };
    };

    SECTION("pre-processor specifiers")
    {
      errors = {
          {R"(
#pack(unknown)

struct s { float a; };

s data;
)",
           1, "packing rule"},
          {R"(
#foo

struct s { float a; };

s data;
)",
           1, "pre-processor"},
          {R"(
#pack
(cbuffer)

struct s { float a; };

s data;
)",
           1, "pre-processor"},
          {R"(
#pack(std140)

struct s {
  float a;
  #pack(scalar)
  float3 b;
};

s data;
)",
           5, "global scope"},
      };
    };

    SECTION("annotation errors")
    {
      errors = {
          {R"(
[[foo]]
struct s { float a; };

s data;
)",
           2, "unrecognised annotation"},
          {R"(
[[pad]]
struct s { float a; };

s data;
)",
           2, "unrecognised annotation"},
          {R"(
[[foo]]
enum e1 : uint { val = 1; };

e1 data;
)",
           2, "unrecognised annotation"},
          {R"(
[[pad]]
enum e1 : uint { val = 1; };

e1 data;
)",
           2, "unrecognised annotation"},
          {R"(
struct s {
  [[size]]float a;
};

s data;
)",
           2, "unrecognised annotation"},
          {R"(
enum e1 : uint {
  [[pad]]val = 1;
};

e1 data;
)",
           2, "unrecognised annotation"},
          {R"(
[[size]]float a;
)",
           1, "unrecognised annotation"},
          {R"(
byte a : 4;
[[size]]byte : 4;
)",
           2, "unrecognised annotation"},

          {R"(
[[size]]
struct s {
  float a;
};

s data;
)",
           2, "requires a parameter"},
          {R"(
[[byte_size]]
struct s {
  float a;
};

s data;
)",
           2, "requires a parameter"},
          {R"(
struct s {
  [[offset]]float a;
};

s data;
)",
           2, "requires a parameter"},
          {R"(
struct s {
  [[byte_offset]]float a;
};

s data;
)",
           2, "requires a parameter"},
          {R"(
[[size(16)]]
struct s {
  float a[100];
};

s data;
)",
           4, "less than derived"},
          {R"(
struct s {
  float a[100];
  [[offset(16)]]
  float b;
};

s data;
)",
           4, "overlaps with"},
          {R"(
struct inner { int a; }

struct s {
  float a[100];
  [[offset(16)]]
  inner b;
};

s data;
)",
           6, "overlaps with"},
          {R"(
struct inner { int a; }

struct s {
  float a[100];
  [[offset(16)]]
  inner *b;
};

s data;
)",
           6, "overlaps with"},
          {R"(
enum e : uint { val = 1; }

struct s {
  float a[100];
  [[offset(16)]]
  e b;
};

s data;
)",
           6, "overlaps with"},
          {R"(
struct s {
  [[packed]]uint4 a;
};

s data;
)",
           2, "requires a parameter"},
          {R"(
struct s {
  [[packed(r1g2b13a16)]]uint4 a;
};

s data;
)",
           2, "unrecognised format"},
          {R"(
[[single]] float a;
[[single]] float b;
)",
           2, "only one"},
          {R"(
[[single]] float a[];
)",
           1, "unbounded"},
          {R"(
struct s {
  [[single]]uint4 a;
};

s data;
)",
           2, "global variables"},
          {R"(
[[single]]
struct s {
  uint4 a;
};

s data;
)",
           6, "only defined"},
          {R"(
[[hex]] float a;
)",
           1, "floating point"},
          {R"(
[[hex]] [[packed(r10g10b10a2)]] uint4 a;
)",
           1, "packed"},
          {R"(
[[bin]] float a;
)",
           1, "floating point"},
          {R"(
[[bin]] [[packed(r10g10b10a2)]] uint4 a;
)",
           1, "packed"},
          {R"(
[[row_major]] float a;
)",
           1, "matrices"},
          {R"(
[[col_major]] float a;
)",
           1, "matrices"},
          {R"(
[[packed(r11g11b10)]] float a;
)",
           1, "float3"},
          {R"(
[[packed(r11g11b10)]] uint3 a;
)",
           1, "float3"},
          {R"(
[[packed(r10g10b10a2)]] float a;
)",
           1, "uint4"},
          {R"(
[[packed(r10g10b10a2)]] float4 a;
)",
           1, "uint4"},
          {R"(
[[packed(r10g10b10a2_unorm)]] float4 a;
)",
           1, "uint4"},
          {R"(
[[packed(r10g10b10a2_snorm)]] uint4 a;
)",
           1, "int4"},
      };
    };

    SECTION("bitfield errors")
    {
      errors = {
          {R"(
struct inner { int val; }

int a : 8;
inner *b : 24;
)",
           4, "packed into a bitfield"},
          {R"(
struct inner { int val; }

int a : 8;
inner b : 24;
)",
           4, "packed into a bitfield"},
          {R"(
int a : 8;
byte b[3] : 24;
)",
           2, "packed into a bitfield"},
          {R"(
int a : 8;
float b : 24;
)",
           2, "packed into a bitfield"},
          {R"(
int a : 8;
byte3 b : 24;
)",
           2, "packed into a bitfield"},
          {R"(
int a : 8;
byte2x2 b : 24;
)",
           2, "packed into a bitfield"},
          {R"(
int a : 8;
[[packed(r10g10b10a2)]] uint4 b : 24;
)",
           2, "packed into a bitfield"},
          {R"(
byte a : 8;
byte b : 16;
)",
           2, "only has 8 bits"},
      };
    };

    SECTION("variable declaration errors")
    {
      errors = {
          {R"(
struct s {
  float a;
};
struct s {
  int a;
};

s data;
)",
           4, "already been"},
          {R"(
enum e {
  val = 1,
};

e data;
)",
           1, "base type"},
          {R"(
enum e : foo {
  val = 1,
};

e data;
)",
           1, "integer type"},
          {R"(
enum e : uint {
  val = blah,
};

e data;
)",
           2, "value declaration"},
          {R"(
enum e : uint {
  val = ,
};

e data;
)",
           2, "value declaration"},
          {R"(
enum e : uint {
  val,
};

e data;
)",
           2, "value declaration"},
          {R"(
enum e : uint {
  val = 1,
  val2 = val+1,
};

e data;
)",
           3, "value declaration"},
          {R"(
struct s {
  float a;
};

s **data;
)",
           5, "single pointer"},
          {R"(
struct s {
  float a;
};

t data;
)",
           5, "unrecognised type"},
          {R"(
blah data;
)",
           1, "unrecognised type"},
          {R"(
struct s {
  float a;
};

s data[2][2];
)",
           5, "invalid declaration"},
          {R"(
float a
int b;
)",
           2, "multiple declarations"},
      };
    };

    for(const error_expect &err : errors)
    {
      parsed = BufferFormatter::ParseFormatString(err.text, 0, true);
      REQUIRE(parsed.errors.contains(err.line));
      // Failed to parse declaration
      CHECK(parsed.errors[err.line].contains(err.error, Qt::CaseInsensitive));
    }
  };
};

#endif
