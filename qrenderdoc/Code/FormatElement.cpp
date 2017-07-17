/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
  else if(f.compType == CompType::UNorm)
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
  else if(f.compType == CompType::UNorm)
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

FormatElement::FormatElement()
{
  buffer = 0;
  offset = 0;
  perinstance = false;
  instancerate = 1;
  rowmajor = false;
  matrixdim = 0;
  hex = false;
  rgb = false;
  systemValue = ShaderBuiltin::Undefined;
}

FormatElement::FormatElement(const QString &Name, int buf, uint offs, bool perInst, int instRate,
                             bool rowMat, uint matDim, ResourceFormat f, bool hexDisplay,
                             bool rgbDisplay)
{
  name = Name;
  buffer = buf;
  offset = offs;
  format = f;
  perinstance = perInst;
  instancerate = instRate;
  rowmajor = rowMat;
  matrixdim = matDim;
  hex = hexDisplay;
  rgb = rgbDisplay;
  systemValue = ShaderBuiltin::Undefined;
}

QList<FormatElement> FormatElement::ParseFormatString(const QString &formatString, uint64_t maxLen,
                                                      bool tightPacking, QString &errors)
{
  QList<FormatElement> elems;

  // regex doesn't account for trailing or preceeding whitespace, or comments

  QRegularExpression regExpr(
      lit("^"                                     // start of the line
          "(row_major\\s+)?"                      // row_major matrix
          "(rgb\\s+)?"                            // rgb element colourising
          "("                                     // group the options for the type
          "uintten|unormten"                      // R10G10B10A2 types
          "|floateleven"                          // R11G11B10 special type
          "|unormh|unormb"                        // UNORM 16-bit and 8-bit types
          "|snormh|snormb"                        // SNORM 16-bit and 8-bit types
          "|bool"                                 // bool is stored as 4-byte int
          "|byte|short|int"                       // signed ints
          "|ubyte|ushort|uint"                    // unsigned ints
          "|xbyte|xshort|xint"                    // hex ints
          "|half|float|double"                    // float types
          "|vec|uvec|ivec"                        // OpenGL vector types
          "|mat|umat|imat"                        // OpenGL matrix types
          ")"                                     // end of the type group
          "([1-9])?"                              // might be a vector
          "(x[1-9])?"                             // or a matrix
          "(\\s+[A-Za-z_][A-Za-z0-9_]*)?"         // get identifier name
          "(\\[[0-9]+\\])?"                       // optional array dimension
          "(\\s*:\\s*[A-Za-z_][A-Za-z0-9_]*)?"    // optional semantic
          "$"));

  bool success = true;
  errors = QString();

  QString text = formatString;

  text = text.replace(lit("{"), QString()).replace(lit("}"), QString());

  QRegularExpression c_comments(lit("/\\*[^*]*\\*+(?:[^*/][^*]*\\*+)*/"));
  QRegularExpression cpp_comments(lit("//.*"));
  text = text.replace(c_comments, QString()).replace(cpp_comments, QString());

  uint32_t offset = 0;

  // get each line and parse it to determine the format the user wanted
  for(QString &l : text.split(QLatin1Char(';')))
  {
    QString line = l.trimmed();

    if(line.isEmpty())
      continue;

    QRegularExpressionMatch match = regExpr.match(line);

    if(!match.hasMatch())
    {
      errors = tr("Couldn't parse line: %1\n").arg(line);
      success = false;
      break;
    }

    QString basetype = match.captured(3);
    bool row_major = !match.captured(1).isEmpty();
    bool rgb = !match.captured(2).isEmpty();
    QString vectorDim = !match.captured(4).isEmpty() ? match.captured(4) : lit("1");
    QString matrixDim = !match.captured(5).isEmpty() ? match.captured(5).mid(1) : lit("1");
    QString name = !match.captured(6).isEmpty() ? match.captured(6).trimmed() : lit("data");
    QString arrayDim = !match.captured(7).isEmpty() ? match.captured(7).trimmed() : lit("[1]");
    arrayDim = arrayDim.mid(1, arrayDim.count() - 2);

    if(!match.captured(5).isEmpty())
    {
      vectorDim.swap(matrixDim);
    }

    ResourceFormat fmt;
    fmt.compType = CompType::Typeless;

    bool hex = false;

    CompType type = CompType::Float;
    uint32_t count = 0;
    uint32_t arrayCount = 1;
    uint32_t matrixCount = 0;
    uint32_t width = 0;

    // check for square matrix declarations like 'mat4' and 'mat3'
    if(basetype == lit("mat") && match.captured(5).isEmpty())
      matrixDim = vectorDim;

    // calculate format
    {
      bool ok = false;

      count = vectorDim.toUInt(&ok);
      if(!ok)
      {
        errors = tr("Invalid vector dimension on line: %1\n").arg(line);
        success = false;
        break;
      }

      arrayCount = arrayDim.toUInt(&ok);
      if(!ok)
      {
        arrayCount = 1;
      }
      arrayCount = qMax(0U, arrayCount);

      matrixCount = matrixDim.toUInt(&ok);
      if(!ok)
      {
        errors = tr("Invalid matrix second dimension on line: %1\n").arg(line);
        success = false;
        break;
      }

      if(basetype == lit("bool"))
      {
        type = CompType::UInt;
        width = 4;
      }
      else if(basetype == lit("byte"))
      {
        type = CompType::SInt;
        width = 1;
      }
      else if(basetype == lit("ubyte") || basetype == lit("xbyte"))
      {
        type = CompType::UInt;
        width = 1;
      }
      else if(basetype == lit("short"))
      {
        type = CompType::SInt;
        width = 2;
      }
      else if(basetype == lit("ushort") || basetype == lit("xshort"))
      {
        type = CompType::UInt;
        width = 2;
      }
      else if(basetype == lit("int") || basetype == lit("ivec") || basetype == lit("imat"))
      {
        type = CompType::SInt;
        width = 4;
      }
      else if(basetype == lit("uint") || basetype == lit("xint") || basetype == lit("uvec") ||
              basetype == lit("umat"))
      {
        type = CompType::UInt;
        width = 4;
      }
      else if(basetype == lit("half"))
      {
        type = CompType::Float;
        width = 2;
      }
      else if(basetype == lit("float") || basetype == lit("vec") || basetype == lit("mat"))
      {
        type = CompType::Float;
        width = 4;
      }
      else if(basetype == lit("double"))
      {
        type = CompType::Double;
        width = 8;
      }
      else if(basetype == lit("unormh"))
      {
        type = CompType::UNorm;
        width = 2;
      }
      else if(basetype == lit("unormb"))
      {
        type = CompType::UNorm;
        width = 1;
      }
      else if(basetype == lit("snormh"))
      {
        type = CompType::SNorm;
        width = 2;
      }
      else if(basetype == lit("snormb"))
      {
        type = CompType::SNorm;
        width = 1;
      }
      else if(basetype == lit("uintten"))
      {
        fmt.compType = CompType::UInt;
        fmt.compCount = 4 * count;
        fmt.compByteWidth = 1;
        fmt.special = true;
        fmt.specialFormat = SpecialFormat::R10G10B10A2;
      }
      else if(basetype == lit("unormten"))
      {
        fmt.compType = CompType::UInt;
        fmt.compCount = 4 * count;
        fmt.compByteWidth = 1;
        fmt.special = true;
        fmt.specialFormat = SpecialFormat::R10G10B10A2;
      }
      else if(basetype == lit("floateleven"))
      {
        fmt.compType = CompType::Float;
        fmt.compCount = 3 * count;
        fmt.compByteWidth = 1;
        fmt.special = true;
        fmt.specialFormat = SpecialFormat::R11G11B10;
      }
      else
      {
        errors = tr("Unrecognised basic type on line: %1\n").arg(line);
        success = false;
        break;
      }
    }

    if(basetype == lit("xint") || basetype == lit("xshort") || basetype == lit("xbyte"))
      hex = true;

    if(fmt.compType == CompType::Typeless)
    {
      fmt.compType = type;
      fmt.compCount = count;
      fmt.compByteWidth = width;
    }

    if(arrayCount == 1)
    {
      FormatElement elem(name, 0, offset, false, 1, row_major, matrixCount, fmt, hex, rgb);

      uint32_t advance = elem.byteSize();

      if(!tightPacking)
      {
        // cbuffer packing always works in floats
        advance = (advance + 3U) & (~3U);

        // cbuffer packing doesn't allow elements to cross float4 boundaries, nudge up if this was
        // the case
        if(offset / 16 != (offset + elem.byteSize() - 1) / 16)
        {
          elem.offset = offset = (offset + 0xFU) & (~0xFU);
        }
      }

      elems.push_back(elem);

      offset += advance;
    }
    else
    {
      // when cbuffer packing, arrays are always aligned at float4 boundary
      if(!tightPacking)
      {
        if(offset % 16 != 0)
        {
          offset = (offset + 0xFU) & (~0xFU);
        }
      }

      for(uint a = 0; a < arrayCount; a++)
      {
        FormatElement elem(QFormatStr("%1[%2]").arg(name).arg(a), 0, offset, false, 1, row_major,
                           matrixCount, fmt, hex, rgb);

        elems.push_back(elem);

        uint32_t advance = elem.byteSize();

        // cbuffer packing each array element is always float4 aligned
        if(!tightPacking)
        {
          advance = (advance + 0xFU) & (~0xFU);
        }

        offset += advance;
      }
    }
  }

  if(!success || elems.isEmpty())
  {
    elems.clear();

    ResourceFormat fmt;
    fmt.compType = CompType::UInt;
    fmt.compByteWidth = 4;
    fmt.compCount = 4;

    if(maxLen > 0 && maxLen < 16)
      fmt.compCount = 1;
    if(maxLen > 0 && maxLen < 4)
      fmt.compByteWidth = 1;

    elems.push_back(FormatElement(lit("data"), 0, 0, false, 1, false, 1, fmt, true, false));
  }

  return elems;
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

QVariantList FormatElement::GetVariants(const byte *&data, const byte *end) const
{
  QVariantList ret;

  bool ok = true;

  if(format.special && format.specialFormat == SpecialFormat::R5G5B5A1)
  {
    uint16_t packed = readObj<uint16_t>(data, end, ok);

    ret.push_back((float)((packed >> 0) & 0x1f) / 31.0f);
    ret.push_back((float)((packed >> 5) & 0x1f) / 31.0f);
    ret.push_back((float)((packed >> 10) & 0x1f) / 31.0f);
    ret.push_back(((packed & 0x8000) > 0) ? 1.0f : 0.0f);

    if(format.bgraOrder)
    {
      QVariant tmp = ret[2];
      ret[2] = ret[0];
      ret[0] = tmp;
    }
  }
  else if(format.special && format.specialFormat == SpecialFormat::R5G6B5)
  {
    uint16_t packed = readObj<uint16_t>(data, end, ok);

    ret.push_back((float)((packed >> 0) & 0x1f) / 31.0f);
    ret.push_back((float)((packed >> 5) & 0x3f) / 63.0f);
    ret.push_back((float)((packed >> 11) & 0x1f) / 31.0f);

    if(format.bgraOrder)
    {
      QVariant tmp = ret[2];
      ret[2] = ret[0];
      ret[0] = tmp;
    }
  }
  else if(format.special && format.specialFormat == SpecialFormat::R4G4B4A4)
  {
    uint16_t packed = readObj<uint16_t>(data, end, ok);

    ret.push_back((float)((packed >> 0) & 0xf) / 15.0f);
    ret.push_back((float)((packed >> 4) & 0xf) / 15.0f);
    ret.push_back((float)((packed >> 8) & 0xf) / 15.0f);
    ret.push_back((float)((packed >> 12) & 0xf) / 15.0f);

    if(format.bgraOrder)
    {
      QVariant tmp = ret[2];
      ret[2] = ret[0];
      ret[0] = tmp;
    }
  }
  else if(format.special && format.specialFormat == SpecialFormat::R10G10B10A2)
  {
    // allow for vectors of this format - for raw buffer viewer
    for(int i = 0; i < int(format.compCount / 4); i++)
    {
      uint32_t packed = readObj<uint32_t>(data, end, ok);

      uint32_t r = (packed >> 0) & 0x3ff;
      uint32_t g = (packed >> 10) & 0x3ff;
      uint32_t b = (packed >> 20) & 0x3ff;
      uint32_t a = (packed >> 30) & 0x003;

      if(format.bgraOrder)
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
      else if(format.compType == CompType::SInt || format.compType == CompType::SScaled)
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
  else if(format.special && format.specialFormat == SpecialFormat::R11G11B10)
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
    int dim = (int)(qMax(matrixdim, 1U) * format.compCount);

    for(int i = 0; i < dim; i++)
    {
      if(format.compType == CompType::Float)
      {
        if(format.compByteWidth == 8)
          ret.push_back(readObj<double>(data, end, ok));
        else if(format.compByteWidth == 4)
          ret.push_back(readObj<float>(data, end, ok));
        else if(format.compByteWidth == 2)
          ret.push_back(RENDERDOC_HalfToFloat(readObj<uint16_t>(data, end, ok)));
      }
      else if(format.compType == CompType::SInt)
      {
        if(format.compByteWidth == 4)
          ret.push_back((int)readObj<int32_t>(data, end, ok));
        else if(format.compByteWidth == 2)
          ret.push_back((int)readObj<int16_t>(data, end, ok));
        else if(format.compByteWidth == 1)
          ret.push_back((int)readObj<int8_t>(data, end, ok));
      }
      else if(format.compType == CompType::UInt)
      {
        if(format.compByteWidth == 4)
          ret.push_back((uint32_t)readObj<uint32_t>(data, end, ok));
        else if(format.compByteWidth == 2)
          ret.push_back((uint32_t)readObj<uint16_t>(data, end, ok));
        else if(format.compByteWidth == 1)
          ret.push_back((uint32_t)readObj<uint8_t>(data, end, ok));
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
      else if(format.compType == CompType::Double)
      {
        ret.push_back(readObj<double>(data, end, ok));
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

    if(format.bgraOrder)
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

ShaderVariable FormatElement::GetShaderVar(const byte *&data, const byte *end) const
{
  QVariantList objs = GetVariants(data, end);

  ShaderVariable ret;

  ret.name = name.toUtf8().data();
  ret.type = VarType::Float;
  if(format.compType == CompType::UInt)
    ret.type = VarType::UInt;
  if(format.compType == CompType::SInt)
    ret.type = VarType::Int;
  if(format.compType == CompType::Double)
    ret.type = VarType::Double;

  ret.columns = qMin(format.compCount, 4U);
  ret.rows = qMin(matrixdim, 4U);

  ret.displayAsHex = hex;

  for(uint32_t row = 0; row < ret.rows; row++)
  {
    for(uint32_t col = 0; col < ret.columns; col++)
    {
      uint32_t dst = row * ret.columns + col;
      uint32_t src = row * format.compCount + col;

      // if we partially read a failure, reset the variable and return
      if((int)src >= objs.size())
      {
        ret.name = "-";
        memset(ret.value.dv, 0, sizeof(ret.value.dv));
        return ret;
      }

      const QVariant &o = objs[src];

      if(ret.type == VarType::Double)
        ret.value.dv[dst] = o.toDouble();
      else if(ret.type == VarType::UInt)
        ret.value.uv[dst] = o.toUInt();
      else if(ret.type == VarType::Int)
        ret.value.iv[dst] = o.toInt();
      else
        ret.value.fv[dst] = o.toFloat();
    }
  }

  return ret;
}

uint32_t FormatElement::byteSize() const
{
  uint32_t vecSize = format.compByteWidth * format.compCount;

  if(format.special)
  {
    if(format.specialFormat == SpecialFormat::R5G5B5A1 ||
       format.specialFormat == SpecialFormat::R5G6B5 ||
       format.specialFormat == SpecialFormat::R4G4B4A4)
      vecSize = 2;

    if(format.specialFormat == SpecialFormat::R10G10B10A2 ||
       format.specialFormat == SpecialFormat::R11G11B10)
      vecSize = 4;
  }

  return vecSize * matrixdim;
}

QString TypeString(const ShaderVariable &v)
{
  if(v.members.count > 0 || v.isStruct)
  {
    if(v.isStruct)
      return lit("struct");
    else
      return QFormatStr("%1[%2]").arg(TypeString(v.members[0])).arg(v.members.count);
  }

  QString typeStr = ToQStr(v.type);

  if(v.displayAsHex && v.type == VarType::UInt)
    typeStr = lit("xint");

  if(v.rows == 1 && v.columns == 1)
    return typeStr;
  if(v.rows == 1)
    return QFormatStr("%1%2").arg(typeStr).arg(v.columns);
  else
    return QFormatStr("%1%2x%3").arg(typeStr).arg(v.rows).arg(v.columns);
}

template <typename el>
static QString RowValuesToString(int cols, el x, el y, el z, el w)
{
  if(cols == 1)
    return Formatter::Format(x);
  else if(cols == 2)
    return QFormatStr("%1, %2").arg(Formatter::Format(x)).arg(Formatter::Format(y));
  else if(cols == 3)
    return QFormatStr("%1, %2, %3")
        .arg(Formatter::Format(x))
        .arg(Formatter::Format(y))
        .arg(Formatter::Format(z));
  else
    return QFormatStr("%1, %2, %3, %4")
        .arg(Formatter::Format(x))
        .arg(Formatter::Format(y))
        .arg(Formatter::Format(z))
        .arg(Formatter::Format(w));
}

QString RowString(const ShaderVariable &v, uint32_t row, VarType type)
{
  if(type == VarType::Unknown)
    type = v.type;

  if(type == VarType::Double)
    return RowValuesToString((int)v.columns, v.value.dv[row * v.columns + 0],
                             v.value.dv[row * v.columns + 1], v.value.dv[row * v.columns + 2],
                             v.value.dv[row * v.columns + 3]);
  else if(type == VarType::Int)
    return RowValuesToString((int)v.columns, v.value.iv[row * v.columns + 0],
                             v.value.iv[row * v.columns + 1], v.value.iv[row * v.columns + 2],
                             v.value.iv[row * v.columns + 3]);
  else if(type == VarType::UInt)
    return RowValuesToString((int)v.columns, v.value.uv[row * v.columns + 0],
                             v.value.uv[row * v.columns + 1], v.value.uv[row * v.columns + 2],
                             v.value.uv[row * v.columns + 3]);
  else
    return RowValuesToString((int)v.columns, v.value.fv[row * v.columns + 0],
                             v.value.fv[row * v.columns + 1], v.value.fv[row * v.columns + 2],
                             v.value.fv[row * v.columns + 3]);
}

QString VarString(const ShaderVariable &v)
{
  if(v.members.count > 0)
    return QString();

  if(v.rows == 1)
    return RowString(v, 0);

  QString ret;
  for(int i = 0; i < (int)v.rows; i++)
  {
    if(i > 0)
      ret += lit(", ");
    ret += lit("{") + RowString(v, i) + lit("}");
  }

  return lit("{ ") + ret + lit(" }");
}

QString RowTypeString(const ShaderVariable &v)
{
  if(v.members.count > 0 || v.isStruct)
  {
    if(v.isStruct)
      return lit("struct");
    else
      return lit("flibbertygibbet");
  }

  if(v.rows == 0 && v.columns == 0)
    return lit("-");

  QString typeStr = ToQStr(v.type);

  if(v.displayAsHex && v.type == VarType::UInt)
    typeStr = lit("xint");

  if(v.columns == 1)
    return typeStr;

  return QFormatStr("%1%2").arg(typeStr).arg(v.columns);
}
