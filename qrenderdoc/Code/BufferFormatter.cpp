/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2022 Baldur Karlsson
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
  ShaderConstant structDef;
  uint32_t pointerTypeId = 0;
  uint32_t offset = 0;
};

GraphicsAPI BufferFormatter::m_API;

static bool MatchBaseTypeDeclaration(QString basetype, const bool isUnsigned, ShaderConstant &el)
{
  if(basetype == lit("bool"))
  {
    el.type.descriptor.type = VarType::Bool;
  }
  else if(basetype == lit("byte") || basetype == lit("char"))
  {
    el.type.descriptor.type = VarType::SByte;

    if(isUnsigned)
      el.type.descriptor.type = VarType::UByte;
  }
  else if(basetype == lit("ubyte") || basetype == lit("xbyte"))
  {
    el.type.descriptor.type = VarType::UByte;
  }
  else if(basetype == lit("short"))
  {
    el.type.descriptor.type = VarType::SShort;

    if(isUnsigned)
      el.type.descriptor.type = VarType::UShort;
  }
  else if(basetype == lit("ushort") || basetype == lit("xshort"))
  {
    el.type.descriptor.type = VarType::UShort;
  }
  else if(basetype == lit("long"))
  {
    el.type.descriptor.type = VarType::SLong;

    if(isUnsigned)
      el.type.descriptor.type = VarType::ULong;
  }
  else if(basetype == lit("ulong") || basetype == lit("xlong"))
  {
    el.type.descriptor.type = VarType::ULong;
  }
  else if(basetype == lit("int") || basetype == lit("ivec") || basetype == lit("imat"))
  {
    el.type.descriptor.type = VarType::SInt;

    if(isUnsigned)
      el.type.descriptor.type = VarType::UInt;
  }
  else if(basetype == lit("uint") || basetype == lit("xint") || basetype == lit("uvec") ||
          basetype == lit("umat"))
  {
    el.type.descriptor.type = VarType::UInt;
  }
  else if(basetype == lit("half"))
  {
    el.type.descriptor.type = VarType::Half;
  }
  else if(basetype == lit("float") || basetype == lit("vec") || basetype == lit("mat"))
  {
    el.type.descriptor.type = VarType::Float;
  }
  else if(basetype == lit("double") || basetype == lit("dvec") || basetype == lit("dmat"))
  {
    el.type.descriptor.type = VarType::Double;
  }
  else if(basetype == lit("unormh"))
  {
    el.type.descriptor.type = VarType::UShort;
    el.type.descriptor.flags |= ShaderVariableFlags::UNorm;
  }
  else if(basetype == lit("unormb"))
  {
    el.type.descriptor.type = VarType::UByte;
    el.type.descriptor.flags |= ShaderVariableFlags::UNorm;
  }
  else if(basetype == lit("snormh"))
  {
    el.type.descriptor.type = VarType::SShort;
    el.type.descriptor.flags |= ShaderVariableFlags::SNorm;
  }
  else if(basetype == lit("snormb"))
  {
    el.type.descriptor.type = VarType::SByte;
    el.type.descriptor.flags |= ShaderVariableFlags::SNorm;
  }
  else if(basetype == lit("uintten"))
  {
    el.type.descriptor.type = VarType::UInt;
    el.type.descriptor.flags |= ShaderVariableFlags::R10G10B10A2;
    el.type.descriptor.columns = 4;
  }
  else if(basetype == lit("unormten"))
  {
    el.type.descriptor.type = VarType::UInt;
    el.type.descriptor.flags |= ShaderVariableFlags::R10G10B10A2;
    el.type.descriptor.flags |= ShaderVariableFlags::UNorm;
    el.type.descriptor.columns = 4;
  }
  else if(basetype == lit("floateleven"))
  {
    el.type.descriptor.type = VarType::Float;
    el.type.descriptor.flags |= ShaderVariableFlags::R11G11B10;
    el.type.descriptor.columns = 3;
  }
  else
  {
    return false;
  }

  return true;
}

ShaderConstant BufferFormatter::ParseFormatString(const QString &formatString, uint64_t maxLen,
                                                  bool tightPacking, QString &errors)
{
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
          ")"                                            // end of the type group
          "(?<vec>[1-9])?"                               // might be a vector
          "(?<mat>x[1-9])?"                              // or a matrix
          "(?<name>\\s+[A-Za-z@_][A-Za-z0-9@_]*)?"       // get identifier name
          "(?<array>\\s*\\[[0-9]+\\])?"                  // optional array dimension
          "(\\s*:\\s*"                                   // optional specifier after :
          "("                                            // bitfield or semantic
          "(?<bitfield>[1-9][0-9]*)|"                    // bitfield packing
          "(?<semantic>[A-Za-z_][A-Za-z0-9_]*)"          // semantic to ignore
          ")"                                            // end bitfield or semantic
          ")?"
          "$"));

  bool success = true;
  errors = QString();

  QString text = formatString;

  QRegularExpression c_comments(lit("/\\*[^*]*\\*+(?:[^*/][^*]*\\*+)*/"));
  QRegularExpression cpp_comments(lit("//.*"));
  // remove all comments
  text = text.replace(c_comments, QString()).replace(cpp_comments, QString());
  // ensure braces are forced onto separate lines so we can parse them
  text = text.replace(QLatin1Char('{'), lit("\n{\n")).replace(QLatin1Char('}'), lit("\n}\n"));
  // treat commas as semi-colons for simplicity of parsing enum declarations and struct declarations
  text = text.replace(QLatin1Char(','), QLatin1Char(';'));

  QRegularExpression annotationRegex(lit("^(\\[\\[[^]]+\\]\\])\\s*"));

  QRegularExpression structDeclRegex(
      lit("^(struct|enum)\\s+([A-Za-z_][A-Za-z0-9_]*)(\\s*:\\s*([a-z]+))?$"));
  QRegularExpression structUseRegex(
      lit("^"                                 // start of the line
          "([A-Za-z_][A-Za-z0-9_]*)"          // struct type name
          "\\s*(\\*)?"                        // maybe a pointer
          "\\s+([A-Za-z@_][A-Za-z0-9@_]*)"    // variable name
          "(\\s*\\[[0-9]+\\])?"               // optional array dimension
          "(\\s*:\\s*([1-9][0-9]*))?"         // optional bitfield packing
          "$"));
  QRegularExpression enumValueRegex(
      lit("^"                           // start of the line
          "([A-Za-z_][A-Za-z0-9_]*)"    // value name
          "\\s*=\\s*"                   // maybe a pointer
          "(0x[0-9a-fA-F]+|[0-9]+)"     // numerical value
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

  uint32_t bitfieldCurPos = ~0U;

  QStringList annotations;

  // get each line and parse it to determine the format the user wanted
  for(QString &l : text.split(QRegularExpression(lit("[;\n\r]"))))
  {
    QString line = l.trimmed();

    if(line.isEmpty())
      continue;

    do
    {
      QRegularExpressionMatch match = annotationRegex.match(line);

      if(!match.hasMatch())
        break;

      annotations.push_back(match.captured(1));

      line.remove(match.capturedStart(0), match.capturedLength(0));
    } while(true);

    if(line.isEmpty())
      continue;

    if(cur == &root)
    {
      // if we're not in a struct, ignore the braces
      if(line == lit("{") || line == lit("}"))
        continue;
    }
    else
    {
      // if we're in a struct, ignore the opening brace and revert back to root elements when we hit
      // the closing brace. No brace nesting is supported
      if(line == lit("{"))
        continue;

      if(line == lit("}"))
      {
        if(bitfieldCurPos != ~0U)
        {
          // update final offset to account for any bits consumed by a trailing bitfield, including
          // any bits in the last byte that weren't allocated
          cur->offset += (bitfieldCurPos + 7) / 8;

          // reset bitpacking state.
          bitfieldCurPos = ~0U;
        }

        if(cur->structDef.type.descriptor.type == VarType::Struct)
        {
          cur->structDef.type.descriptor.arrayByteStride = cur->offset;

          // struct strides are aligned up to float4 boundary
          if(!tightPacking)
            cur->structDef.type.descriptor.arrayByteStride = (cur->offset + 0xFU) & (~0xFU);

          cur->pointerTypeId = PointerTypeRegistry::GetTypeID(cur->structDef.type);
        }

        cur = &root;
        continue;
      }
    }

    if(line.startsWith(lit("struct")) || line.startsWith(lit("enum")))
    {
      if(!annotations.empty())
      {
        errors = tr("Annotations are not supported on struct definitions: %1\n").arg(line);
        success = false;
        break;
      }

      QRegularExpressionMatch match = structDeclRegex.match(line);

      if(match.hasMatch())
      {
        QString name = match.captured(2);

        if(structelems.contains(name))
        {
          errors = tr("Duplicate struct/enum definition: %1\n").arg(name);
          success = false;
          break;
        }

        cur = &structelems[name];
        cur->structDef.type.descriptor.name = name;
        bitfieldCurPos = ~0U;

        if(match.captured(1) == lit("struct"))
        {
          lastStruct = name;
          cur->structDef.type.descriptor.type = VarType::Struct;
        }
        else
        {
          cur->structDef.type.descriptor.type = VarType::Enum;

          QString baseType = match.captured(4);

          if(baseType.isEmpty())
          {
            errors = tr("Enum declarations require sized base type, see line: %1\n").arg(name);
            success = false;
            break;
          }

          ShaderConstant tmp;

          bool matched = MatchBaseTypeDeclaration(baseType, true, tmp);

          if(!matched)
          {
            errors = tr("Unknown enum base type on line: %1\n").arg(line);
            success = false;
            break;
          }

          cur->structDef.type.descriptor.arrayByteStride = VarTypeByteSize(tmp.type.descriptor.type);
        }

        continue;
      }
    }

    ShaderConstant el;

    if(cur->structDef.type.descriptor.type == VarType::Enum)
    {
      QRegularExpressionMatch enumMatch = enumValueRegex.match(line);

      if(!enumMatch.hasMatch())
      {
        errors = tr("Couldn't parse enum value declaration on line: %1\n").arg(line);
        success = false;
        break;
      }

      bool ok = false;
      uint64_t val = enumMatch.captured(2).toULongLong(&ok, 0);

      if(!ok)
      {
        errors = tr("Couldn't parse enum numerical value on line: %1\n").arg(line);
        success = false;
        break;
      }

      el.name = enumMatch.captured(1);
      el.defaultValue = val;

      if(!annotations.empty())
      {
        errors = tr("Annotations are not supported on enum values: %1\n").arg(el.name);
        success = false;
        break;
      }

      cur->structDef.type.members.push_back(el);

      continue;
    }

    QRegularExpressionMatch bitfieldSkipMatch = bitfieldSkipRegex.match(line);

    if(bitfieldSkipMatch.hasMatch())
    {
      if(bitfieldCurPos == ~0U)
        bitfieldCurPos = 0;
      bitfieldCurPos += bitfieldSkipMatch.captured(3).toUInt();

      if(!annotations.empty())
      {
        errors = tr("Annotations are not supported on bitfield skips: %1\n").arg(line);
        success = false;
        break;
      }

      continue;
    }

    QRegularExpressionMatch structMatch = structUseRegex.match(line);

    if(structMatch.hasMatch() && structelems.contains(structMatch.captured(1)))
    {
      StructFormatData &structContext = structelems[structMatch.captured(1)];

      bool isPointer = !structMatch.captured(2).trimmed().isEmpty();

      QString varName = structMatch.captured(3);

      if(!annotations.empty())
      {
        errors = tr("Annotations are not supported on struct %2: %1\n")
                     .arg(varName)
                     .arg(annotations.join(QLatin1Char(' ')));
        success = false;
        break;
      }

      QString arrayDim = structMatch.captured(4).trimmed();
      uint32_t arrayCount = 1;
      if(!arrayDim.isEmpty())
      {
        arrayDim = arrayDim.mid(1, arrayDim.count() - 2);
        bool ok = false;
        arrayCount = arrayDim.toUInt(&ok);
        if(!ok)
          arrayCount = 1;
      }

      QString bitfield = structMatch.captured(6).trimmed();

      if(isPointer)
      {
        if(!bitfield.isEmpty())
        {
          errors = tr("Bitfield packing is not allowed on pointers on line: %1\n").arg(line);
          success = false;
          break;
        }

        // if not tight packing, align up to pointer size
        if(!tightPacking)
          cur->offset = (cur->offset + 0x7) & (~0x7);

        el.name = varName;
        el.byteOffset = cur->offset;
        el.type.descriptor.pointerTypeID = structContext.pointerTypeId;
        el.type.descriptor.type = VarType::ULong;
        el.type.descriptor.flags |= ShaderVariableFlags::HexDisplay;
        el.type.descriptor.arrayByteStride = 8;
        el.type.descriptor.elements = arrayCount;

        cur->offset += 8;
        cur->structDef.type.members.push_back(el);

        continue;
      }
      else if(structContext.structDef.type.descriptor.type == VarType::Enum)
      {
        if(!bitfield.isEmpty() && !arrayDim.isEmpty())
        {
          errors = tr("Bitfield packing is not allowed on arrays on line: %1\n").arg(line);
          success = false;
          break;
        }

        el = structContext.structDef;
        el.name = varName;
        el.byteOffset = cur->offset;
        el.type.descriptor.elements = arrayCount;

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
          errors = tr("Bitfield packing is not allowed on structs on line: %1\n").arg(line);
          success = false;
          break;
        }

        // cbuffer packing rules, structs are always float4 base aligned
        if(!tightPacking)
          cur->offset = (cur->offset + 0xFU) & (~0xFU);

        el = structContext.structDef;
        el.name = varName;
        el.byteOffset = cur->offset;
        el.type.descriptor.elements = arrayCount;

        cur->structDef.type.members.push_back(el);

        // undo the padding after the last struct
        uint32_t padding = el.type.descriptor.arrayByteStride - structContext.offset;

        cur->offset += el.type.descriptor.elements * el.type.descriptor.arrayByteStride - padding;

        continue;
      }
    }
    else
    {
      QRegularExpressionMatch match = regExpr.match(line);

      if(!match.hasMatch())
      {
        errors = tr("Couldn't parse line: %1\n").arg(line);
        success = false;
        break;
      }

      el.name = !match.captured(lit("name")).isEmpty() ? match.captured(lit("name")).trimmed()
                                                       : lit("data");

      QString basetype = match.captured(lit("type"));
      if(match.captured(lit("major")).trimmed() == lit("row_major"))
        el.type.descriptor.flags |= ShaderVariableFlags::RowMajorMatrix;
      if(!match.captured(lit("rgb")).isEmpty())
        el.type.descriptor.flags |= ShaderVariableFlags::RGBDisplay;
      QString firstDim =
          !match.captured(lit("vec")).isEmpty() ? match.captured(lit("vec")) : lit("1");
      QString secondDim =
          !match.captured(lit("mat")).isEmpty() ? match.captured(lit("mat")).mid(1) : lit("1");
      QString arrayDim = !match.captured(lit("array")).isEmpty()
                             ? match.captured(lit("array")).trimmed()
                             : lit("[1]");
      arrayDim = arrayDim.mid(1, arrayDim.count() - 2);

      const bool isUnsigned = match.captured(lit("sign")).trimmed() == lit("unsigned");

      QString bitfield = match.captured(lit("bitfield"));

      if(!bitfield.isEmpty() && !arrayDim.isEmpty())
      {
        errors = tr("Bitfield packing is not allowed on arrays on line: %1\n").arg(line);
        success = false;
        break;
      }

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

        el.type.descriptor.columns = firstDim.toUInt(&ok);
        if(!ok)
        {
          errors = tr("Invalid vector dimension on line: %1\n").arg(line);
          success = false;
          break;
        }

        el.type.descriptor.elements = qMax(1U, arrayDim.toUInt(&ok));
        if(!ok)
          el.type.descriptor.elements = 1;

        el.type.descriptor.rows = qMax(1U, secondDim.toUInt(&ok));
        if(!ok)
        {
          errors = tr("Invalid matrix second dimension on line: %1\n").arg(line);
          success = false;
          break;
        }

        el.bitFieldSize = qMax(1U, bitfield.toUInt(&ok));
        if(!ok)
          el.bitFieldSize = 0;

        // vectors are marked as row-major by convention
        if(el.type.descriptor.rows == 1)
          el.type.descriptor.flags |= ShaderVariableFlags::RowMajorMatrix;

        bool matched = MatchBaseTypeDeclaration(basetype, isUnsigned, el);

        if(!matched)
        {
          errors = tr("Unrecognised type on line: %1\n").arg(line);
          success = false;
          break;
        }
      }

      el.type.descriptor.name = ToStr(el.type.descriptor.type) + vecMatSizeSuffix;

      // process packing annotations first, so we have that information to validate e.g. [[unorm]]
      for(QString annot : annotations)
      {
        if(annot == lit("[[packed(r11g11b10)]]") || annot == lit("[[packed(R11G11B10)]]"))
        {
          if(el.type.descriptor.columns != 3 || el.type.descriptor.type != VarType::Float)
          {
            errors = tr("R11G11B10 packing must be specified on a 'float3' variable: %1\n").arg(line);
            success = false;
            break;
          }

          el.type.descriptor.flags |= ShaderVariableFlags::R11G11B10;
        }
        else if(annot == lit("[[packed(r10g10b10a2)]]") ||
                annot == lit("[[packed(R10G10B10A2)]]") ||
                annot == lit("[[packed(r10g10b10a2_uint)]]") ||
                annot == lit("[[packed(R10G10B10A2_UINT)]]"))
        {
          if(el.type.descriptor.columns != 4 || el.type.descriptor.type != VarType::UInt)
          {
            errors = tr("R10G10B10A2 packing must be specified on a 'uint4' variable "
                        "(optionally with [[unorm]]): %1\n")
                         .arg(line);
            success = false;
            break;
          }

          el.type.descriptor.flags |= ShaderVariableFlags::R10G10B10A2;
        }
        else if(annot == lit("[[packed(r10g10b10a2_unorm)]]") ||
                annot == lit("[[packed(R10G10B10A2_UNORM)]]"))
        {
          if(el.type.descriptor.columns != 4 || el.type.descriptor.type != VarType::UInt)
          {
            errors = tr("R10G10B10A2 packing must be specified on a 'uint4' variable "
                        "(optionally with [[unorm]]): %1\n")
                         .arg(line);
            success = false;
            break;
          }

          el.type.descriptor.flags |= ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::UNorm;
        }
        else if(annot == lit("[[packed(r10g10b10a2_snorm)]]") ||
                annot == lit("[[packed(R10G10B10A2_SNORM)]]"))
        {
          if(el.type.descriptor.columns != 4 ||
             (el.type.descriptor.type != VarType::SInt && el.type.descriptor.type != VarType::UInt))
          {
            errors = tr("R10G10B10A2 packing must be specified on a '[u]int4' variable "
                        "when using [[snorm]]): %1\n")
                         .arg(line);
            success = false;
            break;
          }

          el.type.descriptor.flags |= ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::SNorm;
        }
      }

      if(!success)
        break;

      for(QString annot : annotations)
      {
        if(annot == lit("[[rgb]]"))
        {
          el.type.descriptor.flags |= ShaderVariableFlags::RGBDisplay;
        }
        else if(annot == lit("[[hex]]"))
        {
          if(VarTypeCompType(el.type.descriptor.type) == CompType::Float)
          {
            errors =
                tr("Hex display is not supported on floating point formats on line: %1\n").arg(line);
            success = false;
            break;
          }

          if(el.type.descriptor.flags &
             (ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::R11G11B10))
          {
            errors = tr("Hex display is not supported on packed formats on line: %1\n").arg(line);
            success = false;
            break;
          }

          el.type.descriptor.flags |= ShaderVariableFlags::HexDisplay;

          if(el.type.descriptor.type == VarType::SLong)
            el.type.descriptor.type = VarType::ULong;
          else if(el.type.descriptor.type == VarType::SInt)
            el.type.descriptor.type = VarType::UInt;
          else if(el.type.descriptor.type == VarType::SShort)
            el.type.descriptor.type = VarType::UShort;
          else if(el.type.descriptor.type == VarType::SByte)
            el.type.descriptor.type = VarType::UByte;
        }
        else if(annot == lit("[[unorm]]"))
        {
          if(!(el.type.descriptor.flags & ShaderVariableFlags::R10G10B10A2))
          {
            // verify that we're integer typed and 1 or 2 bytes
            if(el.type.descriptor.type != VarType::UShort &&
               el.type.descriptor.type != VarType::SShort &&
               el.type.descriptor.type != VarType::UByte && el.type.descriptor.type != VarType::SByte)
            {
              errors =
                  tr("UNORM packing is only supported on [u]byte and [u]short types: %1\n").arg(line);
              success = false;
              break;
            }
          }

          el.type.descriptor.flags |= ShaderVariableFlags::UNorm;
        }
        else if(annot == lit("[[snorm]]"))
        {
          if(!(el.type.descriptor.flags & ShaderVariableFlags::R10G10B10A2))
          {
            // verify that we're integer typed and 1 or 2 bytes
            if(el.type.descriptor.type != VarType::UShort &&
               el.type.descriptor.type != VarType::SShort &&
               el.type.descriptor.type != VarType::UByte && el.type.descriptor.type != VarType::SByte)
            {
              errors =
                  tr("SNORM packing is only supported on [u]byte and [u]short types: %1\n").arg(line);
              success = false;
              break;
            }
          }

          el.type.descriptor.flags |= ShaderVariableFlags::SNorm;
        }
        else if(annot == lit("[[row_major]]"))
        {
          if(el.type.descriptor.rows == 1)
          {
            errors = tr("Row major can only be specified on matrices: %1\n").arg(line);
            success = false;
            break;
          }

          el.type.descriptor.flags |= ShaderVariableFlags::RowMajorMatrix;
        }
        else if(annot == lit("[[packed(r11g11b10)]]") || annot == lit("[[packed(R11G11B10)]]") ||
                annot == lit("[[packed(r10g10b10a2)]]") ||
                annot == lit("[[packed(R10G10B10A2)]]") ||
                annot == lit("[[packed(r10g10b10a2_uint)]]") ||
                annot == lit("[[packed(R10G10B10A2_UINT)]]") ||
                annot == lit("[[packed(r10g10b10a2_unorm)]]") ||
                annot == lit("[[packed(R10G10B10A2_UNORM)]]"))
        {
          // already processed
        }
        else
        {
          errors = tr("Unrecognised annotation: %1\n").arg(annot);
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
        if(el.type.descriptor.rows > 1 || el.type.descriptor.columns > 1)
        {
          errors = tr("Bitfield packing only allowed on scalar values on line: %1\n").arg(line);
          success = false;
          break;
        }
        if(el.type.descriptor.elements > 1)
        {
          errors = tr("Bitfield packing not allowed on arrays on line: %1\n").arg(line);
          success = false;
          break;
        }
        if(el.type.descriptor.flags &
           (ShaderVariableFlags::R10G10B10A2 | ShaderVariableFlags::R11G11B10 |
            ShaderVariableFlags::UNorm | ShaderVariableFlags::SNorm))
        {
          errors =
              tr("Bitfield packing not allowed on interpreted/packed formats on line: %1\n").arg(line);
          success = false;
          break;
        }
        if(VarTypeCompType(el.type.descriptor.type) == CompType::Float)
        {
          errors =
              tr("Bitfield packing not allowed on floating point formats on line: %1\n").arg(line);
          success = false;
          break;
        }
      }

      if(basetype == lit("xlong") || basetype == lit("xint") || basetype == lit("xshort") ||
         basetype == lit("xbyte"))
        el.type.descriptor.flags |= ShaderVariableFlags::HexDisplay;
    }

    ResourceFormat fmt = GetInterpretedResourceFormat(el);

    // normally the array stride is the size of an element
    const uint32_t elemScalarByteSize = fmt.ElementSize();
    el.type.descriptor.arrayByteStride = el.type.descriptor.matrixByteStride = elemScalarByteSize;

    if(el.bitFieldSize > 0)
    {
      const uint32_t elemScalarBitSize = elemScalarByteSize * 8;

      // bitfields can't be larger than the base type
      if(el.bitFieldSize > elemScalarBitSize)
      {
        errors =
            tr("Bitfield cannot specify a larger size than the base type on line: %1\n").arg(line);
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
        // align the offset up to where this bitfield needs to start
        cur->offset += ((bitfieldCurPos + (elemScalarBitSize - 1)) / elemScalarBitSize) *
                       (elemScalarBitSize / 8);
        // reset the current bitfield pos
        bitfieldCurPos = 0;
      }
    }

    uint32_t padding = 0;

    // for matrices, it's the size of an element times the number of rows
    if(el.type.descriptor.rows > 1)
    {
      // if we're cbuffer packing, matrix row/columns are always 16-bytes apart
      if(!tightPacking)
      {
        padding = 16 - el.type.descriptor.matrixByteStride;
        el.type.descriptor.matrixByteStride = 16;
      }

      uint8_t majorDim =
          el.type.descriptor.RowMajor() ? el.type.descriptor.rows : el.type.descriptor.columns;

      // total matrix size is
      el.type.descriptor.arrayByteStride = el.type.descriptor.matrixByteStride * majorDim;
    }

    el.byteOffset = cur->offset;
    bool updateCurOffset = true;

    // handle bitfield packing
    if(el.bitFieldSize > 0)
    {
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

      // don't update the current position
      updateCurOffset = false;
    }
    else
    {
      // this element is not bitpacked

      // update offset to account for any bits consumed by the previous bitfield, which won't have
      // happened yet, including any bits in the last byte that weren't allocated
      cur->offset += (bitfieldCurPos + 7) / 8;

      // align to our base element size
      cur->offset = (cur->offset + (elemScalarByteSize - 1)) & (~(elemScalarByteSize - 1));

      // reset bitpacking state.
      bitfieldCurPos = ~0U;
    }

    // cbuffer packing rules
    if(!tightPacking && bitfieldCurPos == ~0U)
    {
      if(el.type.descriptor.elements == 1)
      {
        // always float aligned
        el.type.descriptor.arrayByteStride = (el.type.descriptor.arrayByteStride + 3U) & (~3U);

        // elements can't cross float4 boundaries, nudge up if this was the case
        if(cur->offset / 16 != (cur->offset + el.type.descriptor.arrayByteStride - 1) / 16)
        {
          cur->offset = (cur->offset + 0xFU) & (~0xFU);
        }
      }
      else
      {
        // arrays always have elements float4 aligned
        uint32_t paddedStride = (el.type.descriptor.arrayByteStride + 0xFU) & (~0xFU);

        padding += paddedStride - el.type.descriptor.arrayByteStride;

        el.type.descriptor.arrayByteStride = paddedStride;

        // and always aligned at float4 boundary
        if(cur->offset % 16 != 0)
        {
          cur->offset = (cur->offset + 0xFU) & (~0xFU);
        }
      }
    }

    cur->structDef.type.members.push_back(el);

    if(updateCurOffset)
      cur->offset += el.type.descriptor.arrayByteStride * el.type.descriptor.elements - padding;
  }

  if(bitfieldCurPos != ~0U)
  {
    // update final offset to account for any bits consumed by a trailing bitfield, including any
    // bits in the last byte that weren't allocated
    cur->offset += (bitfieldCurPos + 7) / 8;

    // reset bitpacking state.
    bitfieldCurPos = ~0U;
  }

  // if we succeeded parsing but didn't get any root elements, use the last defined struct as the
  // definition
  if(success && root.structDef.type.members.isEmpty() && !lastStruct.isEmpty())
    root = structelems[lastStruct];

  root.structDef.type.descriptor.arrayByteStride = root.offset;

  // struct strides are aligned up to float4 boundary
  if(!tightPacking)
    root.structDef.type.descriptor.arrayByteStride = (root.offset + 0xFU) & (~0xFU);

  if(!success || root.structDef.type.members.isEmpty())
  {
    root.structDef.type.members.clear();

    ShaderConstant el;
    el.byteOffset = 0;
    el.type.descriptor.flags |= ShaderVariableFlags::HexDisplay;
    el.name = "data";
    el.type.descriptor.type = VarType::UInt;
    el.type.descriptor.columns = 4;

    if(maxLen > 0 && maxLen < 16)
      el.type.descriptor.columns = 1;
    if(maxLen > 0 && maxLen < 4)
      el.type.descriptor.type = VarType::UByte;

    el.type.descriptor.arrayByteStride = el.type.descriptor.matrixByteStride =
        el.type.descriptor.columns * VarTypeByteSize(el.type.descriptor.type);

    root.structDef.type.members.push_back(el);
  }

  return root.structDef;
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
    case ResourceFormatType::PVRTC:
      baseType = lit("[[row_major]] [[hex]] int2");
      break;
    // 4x4 byte block, for 128-bit block formats
    case ResourceFormatType::BC2:
    case ResourceFormatType::BC3:
    case ResourceFormatType::BC5:
    case ResourceFormatType::BC6:
    case ResourceFormatType::BC7:
    case ResourceFormatType::ASTC: baseType = lit("[[row_major]] [[hex]] int4"); break;
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

  return QFormatStr("%1 %2[%3];").arg(baseType).arg(varName).arg(w);
}

QString BufferFormatter::GetBufferFormatString(const ShaderResource &res,
                                               const ResourceFormat &viewFormat,
                                               uint64_t &baseByteOffset)
{
  QString format;

  if(!res.variableType.members.empty())
  {
    QList<QString> declaredStructs;
    if(m_API == GraphicsAPI::Vulkan || m_API == GraphicsAPI::OpenGL)
    {
      const rdcarray<ShaderConstant> &members = res.variableType.members;

      // if there is only one member in the root array, we can just call DeclareStruct directly
      if(members.count() <= 1)
      {
        format = DeclareStruct(declaredStructs, res.name, members, 0, QString());
      }
      else
      {
        // otherwise we need to build up the comment indicating which fixed-size members we
        // skipped
        QString fixedPrefixString = tr("    // members skipped as they are fixed size:\n");
        baseByteOffset += members.back().byteOffset;

        // list each member before the last, commented out.
        for(int i = 0; i < members.count() - 1; i++)
        {
          QString arraySize;
          if(members[i].type.descriptor.elements > 1 && members[i].type.descriptor.elements != ~0U)
            arraySize = QFormatStr("[%1]").arg(members[i].type.descriptor.elements);

          QString varName = members[i].name;

          if(varName.isEmpty())
            varName = QFormatStr("_child%1").arg(i);

          fixedPrefixString += QFormatStr("    // %1 %2%3;\n")
                                   .arg(members[i].type.descriptor.name)
                                   .arg(varName)
                                   .arg(arraySize);
        }

        fixedPrefixString +=
            lit("    // final array struct @ byte offset %1\n").arg(members.back().byteOffset);

        // construct a fake list of members with only the last arrayed one, to pass to
        // DeclareStruct
        rdcarray<ShaderConstant> fakeLastMember;
        fakeLastMember.push_back(members.back());
        // rebase offset of this member to 0 so that DeclareStruct doesn't think any padding is
        // needed
        fakeLastMember[0].byteOffset = 0;

        format = DeclareStruct(declaredStructs, res.name, fakeLastMember,
                               fakeLastMember[0].type.descriptor.arrayByteStride, fixedPrefixString);
      }
    }
    else
    {
      format = DeclareStruct(declaredStructs, res.variableType.descriptor.name,
                             res.variableType.members, 0, QString());
    }
  }
  else
  {
    const auto &desc = res.variableType.descriptor;

    if(viewFormat.type == ResourceFormatType::Undefined)
    {
      if(desc.type == VarType::Unknown)
      {
        format = desc.name;
      }
      else
      {
        if(desc.RowMajor() && desc.rows > 1 && desc.columns > 1)
          format += lit("[[row_major]] ");

        format += ToQStr(desc.type);
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
        format = lit("[[packed(r11g11b10]] float3");
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

uint32_t BufferFormatter::GetVarSize(const ShaderConstant &var)
{
  uint32_t size = var.type.descriptor.rows * var.type.descriptor.columns;
  uint32_t typeSize = VarTypeByteSize(var.type.descriptor.type);
  if(typeSize > 1)
    size *= typeSize;

  if(var.type.descriptor.type == VarType::Enum)
    size = var.type.descriptor.arrayByteStride;

  if(var.type.descriptor.rows > 1)
  {
    if(var.type.descriptor.RowMajor())
      size = var.type.descriptor.matrixByteStride * var.type.descriptor.rows;
    else
      size = var.type.descriptor.matrixByteStride * var.type.descriptor.columns;
  }

  if(var.type.descriptor.type != VarType::Enum && !var.type.members.empty())
    size = GetStructVarSize(var.type.members);

  if(var.type.descriptor.elements > 1 && var.type.descriptor.elements != ~0U)
    size *= var.type.descriptor.elements;

  return size;
}

uint32_t BufferFormatter::GetStructVarSize(const rdcarray<ShaderConstant> &members)
{
  uint32_t lastMemberStart = 0;

  const ShaderConstant *lastChild = &members.back();

  lastMemberStart += lastChild->byteOffset;
  while(lastChild->type.descriptor.type != VarType::Enum && !lastChild->type.members.isEmpty())
  {
    if(lastChild->type.descriptor.elements != ~0U)
      lastMemberStart += (qMax(lastChild->type.descriptor.elements, 1U) - 1) *
                         lastChild->type.descriptor.arrayByteStride;
    lastChild = &lastChild->type.members.back();
    lastMemberStart += lastChild->byteOffset;
  }

  return lastMemberStart + GetVarSize(*lastChild);
}

QString BufferFormatter::DeclarePaddingBytes(uint32_t bytes)
{
  if(bytes == 0)
    return QString();

  QString ret;

  if(bytes > 4)
  {
    ret += lit("[[hex]] int pad[%1];").arg(bytes / 4);

    bytes = bytes % 4;
  }

  if(bytes == 4)
    ret += lit("[[hex]] int pad;");
  else if(bytes == 3)
    ret += lit("[[hex]] short pad; [[hex]] byte pad;");
  else if(bytes == 2)
    ret += lit("[[hex]] short pad;");
  else if(bytes == 1)
    ret += lit("[[hex]] xbyte pad;");

  return ret + lit("\n");
}

QString BufferFormatter::DeclareStruct(QList<QString> &declaredStructs, const QString &name,
                                       const rdcarray<ShaderConstant> &members,
                                       uint32_t requiredByteStride, QString innerSkippedPrefixString)
{
  QString ret;

  ret = lit("struct %1\n{\n").arg(name);

  ret += innerSkippedPrefixString;

  uint32_t offset = 0;

  for(int i = 0; i < members.count(); i++)
  {
    if(offset < members[i].byteOffset)
      ret += lit("    ") + DeclarePaddingBytes(members[i].byteOffset - offset);
    else if(offset > members[i].byteOffset)
      qCritical() << "Unexpected offset overlow at" << QString(members[i].name) << "in"
                  << QString(name);

    offset = members[i].byteOffset + GetVarSize(members[i]);

    QString arraySize;
    if(members[i].type.descriptor.elements > 1 && members[i].type.descriptor.elements != ~0U)
      arraySize = QFormatStr("[%1]").arg(members[i].type.descriptor.elements);

    QString varTypeName = members[i].type.descriptor.name;

    if(members[i].type.descriptor.pointerTypeID != ~0U)
    {
      const ShaderConstantType &pointeeType =
          PointerTypeRegistry::GetTypeDescriptor(members[i].type.descriptor.pointerTypeID);

      varTypeName = pointeeType.descriptor.name;

      if(!declaredStructs.contains(varTypeName))
      {
        declaredStructs.push_back(varTypeName);
        ret = DeclareStruct(declaredStructs, varTypeName, pointeeType.members,
                            pointeeType.descriptor.arrayByteStride, QString()) +
              lit("\n") + ret;
      }

      varTypeName += lit("*");
    }
    else if(!members[i].type.members.isEmpty())
    {
      // GL structs don't give us typenames (boo!) so give them unique names. This will mean some
      // structs get duplicated if they're used in multiple places, but not much we can do about
      // that.
      if(varTypeName.isEmpty() || varTypeName == lit("struct"))
        varTypeName = lit("anon%1").arg(declaredStructs.size());

      if(!declaredStructs.contains(varTypeName))
      {
        declaredStructs.push_back(varTypeName);
        ret = DeclareStruct(declaredStructs, varTypeName, members[i].type.members,
                            members[i].type.descriptor.arrayByteStride, QString()) +
              lit("\n") + ret;
      }
    }

    QString varName = members[i].name;

    if(varName.isEmpty())
      varName = QFormatStr("_child%1").arg(i);

    if(members[i].type.descriptor.rows > 1)
    {
      if(members[i].type.descriptor.RowMajor())
      {
        varTypeName = lit("[[row_major]] ") + varTypeName;

        uint32_t tightStride =
            VarTypeByteSize(members[i].type.descriptor.type) * members[i].type.descriptor.columns;

        if(tightStride < members[i].type.descriptor.matrixByteStride)
        {
          uint32_t padSize = members[i].type.descriptor.matrixByteStride - tightStride;
          for(uint32_t r = 0; r < members[i].type.descriptor.rows; r++)
          {
            ret += QFormatStr("    %1%2 %3_row%4; %5")
                       .arg(ToQStr(members[i].type.descriptor.type))
                       .arg(members[i].type.descriptor.columns)
                       .arg(varName)
                       .arg(r)
                       .arg(DeclarePaddingBytes(padSize));
          }

          continue;
        }
      }
      else
      {
        uint32_t tightStride =
            VarTypeByteSize(members[i].type.descriptor.type) * members[i].type.descriptor.rows;

        if(tightStride < members[i].type.descriptor.matrixByteStride)
        {
          uint32_t padSize = members[i].type.descriptor.matrixByteStride - tightStride;
          for(uint32_t c = 0; c < members[i].type.descriptor.columns; c++)
          {
            ret += QFormatStr("    %1%2 %3_col%4; %5")
                       .arg(ToQStr(members[i].type.descriptor.type))
                       .arg(members[i].type.descriptor.rows)
                       .arg(varName)
                       .arg(c)
                       .arg(DeclarePaddingBytes(padSize));
          }

          continue;
        }
      }
    }

    ret += QFormatStr("    %1 %2%3;\n").arg(varTypeName).arg(varName).arg(arraySize);
  }

  if(requiredByteStride > 0)
  {
    const uint32_t structEnd = GetStructVarSize(members);

    if(requiredByteStride > structEnd)
      ret += lit("    ") + DeclarePaddingBytes(requiredByteStride - structEnd);
    else if(requiredByteStride != structEnd)
      qCritical() << "Unexpected stride overlow at struct" << name;
  }

  ret += lit("}\n");

  return ret;
}

QString BufferFormatter::DeclareStruct(const QString &name, const rdcarray<ShaderConstant> &members,
                                       uint32_t requiredByteStride)
{
  QList<QString> declaredStructs;
  return DeclareStruct(declaredStructs, name, members, requiredByteStride, QString());
}

ResourceFormat GetInterpretedResourceFormat(const ShaderConstant &elem)
{
  ResourceFormat format;
  format.type = ResourceFormatType::Regular;

  if(elem.type.descriptor.flags & ShaderVariableFlags::R10G10B10A2)
    format.type = ResourceFormatType::R10G10B10A2;
  else if(elem.type.descriptor.flags & ShaderVariableFlags::R11G11B10)
    format.type = ResourceFormatType::R11G11B10;

  format.compType = VarTypeCompType(elem.type.descriptor.type);

  if(elem.type.descriptor.flags & ShaderVariableFlags::UNorm)
    format.compType = CompType::UNorm;
  else if(elem.type.descriptor.flags & ShaderVariableFlags::SNorm)
    format.compType = CompType::SNorm;

  format.compByteWidth = VarTypeByteSize(elem.type.descriptor.type);

  if(elem.type.descriptor.type == VarType::Enum)
    format.compByteWidth = elem.type.descriptor.arrayByteStride;

  if(elem.type.descriptor.RowMajor() || elem.type.descriptor.rows == 1)
    format.compCount = elem.type.descriptor.columns;
  else
    format.compCount = elem.type.descriptor.rows;

  // packed formats with fixed component counts multiply up the component count
  switch(format.type)
  {
    case ResourceFormatType::R10G10B10A2:
    case ResourceFormatType::R5G5B5A1:
    case ResourceFormatType::R4G4B4A4: format.compCount *= 4; break;
    case ResourceFormatType::R11G11B10:
    case ResourceFormatType::R9G9B9E5:
    case ResourceFormatType::R5G6B5: format.compCount *= 3; break;
    case ResourceFormatType::R4G4: format.compCount *= 2; break;
    default: break;
  }

  return format;
}

static void FillShaderVarData(ShaderVariable &var, const ShaderConstant &elem, const byte *data,
                              const byte *end)
{
  int src = 0;

  uint32_t outerCount = elem.type.descriptor.rows;
  uint32_t innerCount = elem.type.descriptor.columns;

  bool colMajor = false;

  if(elem.type.descriptor.ColMajor() && outerCount > 1)
  {
    colMajor = true;
    std::swap(outerCount, innerCount);
  }

  QVariantList objs = GetVariants(GetInterpretedResourceFormat(elem), elem, data, end);

  if(objs.isEmpty())
  {
    var.name = "-";
    var.value = ShaderValue();
    return;
  }

  for(uint32_t outer = 0; outer < outerCount; outer++)
  {
    for(uint32_t inner = 0; inner < innerCount; inner++)
    {
      uint32_t dst = outer * elem.type.descriptor.columns + inner;

      if(colMajor)
        dst = inner * elem.type.descriptor.columns + outer;

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
        case VarType::Enum:
        case VarType::GPUPointer:
          // treat this as a 64-bit unsigned integer
          var.value.u64v[dst] = o.toULongLong();
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
  ret.type = elem.type.descriptor.type;
  ret.columns = qMin(elem.type.descriptor.columns, uint8_t(4));
  ret.rows = qMin(elem.type.descriptor.rows, uint8_t(4));

  ret.flags = elem.type.descriptor.flags;

  if(elem.type.descriptor.type != VarType::Enum && !elem.type.members.isEmpty())
  {
    ret.rows = ret.columns = 0;

    if(elem.type.descriptor.elements > 1 && elem.type.descriptor.elements != ~0U)
    {
      rdcarray<ShaderVariable> arrayElements;

      for(uint32_t a = 0; a < elem.type.descriptor.elements; a++)
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

        data += elem.type.descriptor.arrayByteStride;
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
  else if(elem.type.descriptor.elements > 1 && elem.type.descriptor.elements != ~0U)
  {
    rdcarray<ShaderVariable> arrayElements;

    for(uint32_t a = 0; a < elem.type.descriptor.elements; a++)
    {
      arrayElements.push_back(ret);
      arrayElements.back().name = QFormatStr("%1[%2]").arg(ret.name).arg(a);
      FillShaderVarData(arrayElements.back(), elem, data, end);
      data += elem.type.descriptor.arrayByteStride;
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
  const ShaderConstantDescriptor &varDesc = var.type.descriptor;

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
        (packed >> 0) & 0x3f, (packed >> 11) & 0x3f, (packed >> 22) & 0x1f,
    };
    int32_t exponents[] = {
        int32_t(packed >> 6) & 0x1f, int32_t(packed >> 17) & 0x1f, int32_t(packed >> 27) & 0x1f,
    };
    static const uint32_t leadbit[] = {
        0x40, 0x40, 0x20,
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

    uint32_t rowCount = varDesc.rows;
    uint32_t colCount = varDesc.columns;

    for(uint32_t row = 0; row < qMax(rowCount, 1U); row++)
    {
      for(uint32_t col = 0; col < qMax(colCount, 1U); col++)
      {
        if(varDesc.RowMajor() || rowCount == 1)
          data = base + row * varDesc.matrixByteStride + col * format.compByteWidth;
        else
          data = base + col * varDesc.matrixByteStride + row * format.compByteWidth;

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

            ret.push_back(val);
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

            ret.push_back(val);
          }

          if(var.type.descriptor.type == VarType::Enum)
          {
            uint64_t val = ret.back().toULongLong();

            QString str = QApplication::translate("BufferFormatter", "Unknown %1 (%2)")
                              .arg(QString(var.type.descriptor.name))
                              .arg(val);

            for(size_t i = 0; i < var.type.members.size(); i++)
            {
              if(val == var.type.members[i].defaultValue)
              {
                str = var.type.members[i].name;
                break;
              }
            }

            ret.back() = str;
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

QString TypeString(const ShaderVariable &v)
{
  if(!v.members.isEmpty() || v.type == VarType::Struct)
  {
    if(v.type == VarType::Struct)
    {
      if(!v.members.empty() && v.members[0].name.contains('['))
        return lit("struct[%2]").arg(v.members.count());
      else
        return lit("struct");
    }
    else
    {
      return QFormatStr("%1[%2]").arg(TypeString(v.members[0])).arg(v.members.count());
    }
  }

  if(v.type == VarType::GPUPointer)
    return PointerTypeRegistry::GetTypeDescriptor(v.GetPointer()).descriptor.name + "*";

  QString typeStr = ToQStr(v.type);

  if(v.type == VarType::ReadOnlyResource)
    typeStr = lit("Resource");
  else if(v.type == VarType::ReadWriteResource)
    typeStr = lit("RW Resource");
  else if(v.type == VarType::Sampler)
    typeStr = lit("Sampler");
  else if(v.type == VarType::ConstantBlock)
    typeStr = lit("Constant Block");

  if(v.flags & ShaderVariableFlags::HexDisplay)
  {
    if(v.type == VarType::ULong)
      typeStr = lit("[[hex]] long");
    else if(v.type == VarType::UInt)
      typeStr = lit("[[hex]] int");
    else if(v.type == VarType::UShort)
      typeStr = lit("[[hex]] short");
    else if(v.type == VarType::UByte)
      typeStr = lit("[[hex]] byte");
  }

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
  const bool hex = bool(flags & ShaderVariableFlags::HexDisplay);

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

QString VarString(const ShaderVariable &v)
{
  if(!v.members.isEmpty())
    return QString();

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
    return PointerTypeRegistry::GetTypeDescriptor(v.GetPointer()).descriptor.name + "*";

  QString typeStr = ToQStr(v.type);

  if(v.flags & ShaderVariableFlags::HexDisplay)
  {
    if(v.type == VarType::ULong)
      typeStr = lit("[[hex]] long");
    else if(v.type == VarType::UInt)
      typeStr = lit("[[hex]] int");
    else if(v.type == VarType::UShort)
      typeStr = lit("[[hex]] short");
    else if(v.type == VarType::UByte)
      typeStr = lit("[[hex]] byte");
  }

  if(v.columns == 1)
    return typeStr;

  return QFormatStr("%1%2").arg(typeStr).arg(v.columns);
}
