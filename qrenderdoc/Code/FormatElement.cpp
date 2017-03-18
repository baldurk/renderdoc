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

#include <QRegularExpression>
#include <QtMath>
#include "QRDUtils.h"

static QVariant interpret(const ResourceFormat &f, uint16_t comp)
{
  if(f.compByteWidth != 2 || f.compType == eCompType_Float)
    return QVariant();

  if(f.compType == eCompType_SInt)
  {
    return (int16_t)comp;
  }
  else if(f.compType == eCompType_UInt)
  {
    return comp;
  }
  else if(f.compType == eCompType_SScaled)
  {
    return (float)((int16_t)comp);
  }
  else if(f.compType == eCompType_UScaled)
  {
    return (float)comp;
  }
  else if(f.compType == eCompType_UNorm)
  {
    return (float)comp / (float)0xffff;
  }
  else if(f.compType == eCompType_SNorm)
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
  if(f.compByteWidth != 1 || f.compType == eCompType_Float)
    return QVariant();

  if(f.compType == eCompType_SInt)
  {
    return (int8_t)comp;
  }
  else if(f.compType == eCompType_UInt)
  {
    return comp;
  }
  else if(f.compType == eCompType_SScaled)
  {
    return (float)((int8_t)comp);
  }
  else if(f.compType == eCompType_UScaled)
  {
    return (float)comp;
  }
  else if(f.compType == eCompType_UNorm)
  {
    return ((float)comp) / 255.0f;
  }
  else if(f.compType == eCompType_SNorm)
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
  name = "";
  buffer = 0;
  offset = 0;
  perinstance = false;
  instancerate = 1;
  rowmajor = false;
  matrixdim = 0;
  hex = false;
  systemValue = eAttr_None;
}

FormatElement::FormatElement(const QString &Name, int buf, uint offs, bool perInst, int instRate,
                             bool rowMat, uint matDim, ResourceFormat f, bool hexDisplay)
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
  systemValue = eAttr_None;
}

QList<FormatElement> FormatElement::ParseFormatString(const QString &formatString, uint64_t maxLen,
                                                      bool tightPacking, QString &errors)
{
  QList<FormatElement> elems;

  // regex doesn't account for trailing or preceeding whitespace, or comments

  QRegularExpression regExpr(
      "^(row_major\\s+)?"    // row_major matrix
      "("
      "uintten|unormten"
      "|unormh|unormb"
      "|snormh|snormb"
      "|bool"                 // bool is stored as 4-byte int
      "|byte|short|int"       // signed ints
      "|ubyte|ushort|uint"    // unsigned ints
      "|xbyte|xshort|xint"    // hex ints
      "|half|float|double"    // float types
      "|vec|uvec|ivec"        // OpenGL vector types
      "|mat|umat|imat"        // OpenGL matrix types
      ")"
      "([1-9])?"                              // might be a vector
      "(x[1-9])?"                             // or a matrix
      "(\\s+[A-Za-z_][A-Za-z0-9_]*)?"         // get identifier name
      "(\\[[0-9]+\\])?"                       // optional array dimension
      "(\\s*:\\s*[A-Za-z_][A-Za-z0-9_]*)?"    // optional semantic
      "$");

  bool success = true;
  errors = "";

  QString text = formatString;

  text = text.replace("{", "").replace("}", "");

  QRegularExpression c_comments("/\\*[^*]*\\*+(?:[^*/][^*]*\\*+)*/");
  QRegularExpression cpp_comments("//.*");
  text = text.replace(c_comments, "").replace(cpp_comments, "");

  uint32_t offset = 0;

  // get each line and parse it to determine the format the user wanted
  for(QString &l : text.split(QChar(';')))
  {
    QString line = l.trimmed();

    if(line.isEmpty())
      continue;

    QRegularExpressionMatch match = regExpr.match(line);

    if(!match.hasMatch())
    {
      errors = "Couldn't parse line:\n" + line;
      success = false;
      break;
    }

    QString basetype = match.captured(2);
    bool row_major = !match.captured(1).isEmpty();
    QString vectorDim = !match.captured(3).isEmpty() ? match.captured(3) : "1";
    QString matrixDim = !match.captured(4).isEmpty() ? match.captured(4).mid(1) : "1";
    QString name = !match.captured(5).isEmpty() ? match.captured(5).trimmed() : "data";
    QString arrayDim = !match.captured(6).isEmpty() ? match.captured(6).trimmed() : "[1]";
    arrayDim = arrayDim.mid(1, arrayDim.count() - 2);

    if(!match.captured(4).isEmpty())
    {
      vectorDim.swap(matrixDim);
    }

    ResourceFormat fmt;
    fmt.compType = eCompType_None;

    bool hex = false;

    FormatComponentType type = eCompType_Float;
    uint32_t count = 0;
    uint32_t arrayCount = 1;
    uint32_t matrixCount = 0;
    uint32_t width = 0;

    // check for square matrix declarations like 'mat4' and 'mat3'
    if(basetype == "mat" && !match.captured(4).isEmpty())
      matrixDim = vectorDim;

    // calculate format
    {
      bool ok = false;

      count = vectorDim.toUInt(&ok);
      if(!ok)
      {
        errors = "Invalid vector dimension on line:\n" + line;
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
        errors = "Invalid matrix second dimension on line:\n" + line;
        success = false;
        break;
      }

      if(basetype == "bool")
      {
        type = eCompType_UInt;
        width = 4;
      }
      else if(basetype == "byte")
      {
        type = eCompType_SInt;
        width = 1;
      }
      else if(basetype == "ubyte" || basetype == "xbyte")
      {
        type = eCompType_UInt;
        width = 1;
      }
      else if(basetype == "short")
      {
        type = eCompType_SInt;
        width = 2;
      }
      else if(basetype == "ushort" || basetype == "xshort")
      {
        type = eCompType_UInt;
        width = 2;
      }
      else if(basetype == "int" || basetype == "ivec" || basetype == "imat")
      {
        type = eCompType_SInt;
        width = 4;
      }
      else if(basetype == "uint" || basetype == "xint" || basetype == "uvec" || basetype == "umat")
      {
        type = eCompType_UInt;
        width = 4;
      }
      else if(basetype == "half")
      {
        type = eCompType_Float;
        width = 2;
      }
      else if(basetype == "float" || basetype == "vec" || basetype == "mat")
      {
        type = eCompType_Float;
        width = 4;
      }
      else if(basetype == "double")
      {
        type = eCompType_Double;
        width = 8;
      }
      else if(basetype == "unormh")
      {
        type = eCompType_UNorm;
        width = 2;
      }
      else if(basetype == "unormb")
      {
        type = eCompType_UNorm;
        width = 1;
      }
      else if(basetype == "snormh")
      {
        type = eCompType_SNorm;
        width = 2;
      }
      else if(basetype == "snormb")
      {
        type = eCompType_SNorm;
        width = 1;
      }
      else if(basetype == "uintten")
      {
        fmt.compType = eCompType_UInt;
        fmt.compCount = 4 * count;
        fmt.compByteWidth = 1;
        fmt.special = true;
        fmt.specialFormat = eSpecial_R10G10B10A2;
      }
      else if(basetype == "unormten")
      {
        fmt.compType = eCompType_UInt;
        fmt.compCount = 4 * count;
        fmt.compByteWidth = 1;
        fmt.special = true;
        fmt.specialFormat = eSpecial_R10G10B10A2;
      }
      else
      {
        errors = "Unrecognised basic type on line:\n" + line;
        success = false;
        break;
      }
    }

    if(basetype == "xint" || basetype == "xshort" || basetype == "xbyte")
      hex = true;

    if(fmt.compType == eCompType_None)
    {
      fmt.compType = type;
      fmt.compCount = count;
      fmt.compByteWidth = width;
    }

    if(arrayCount == 1)
    {
      FormatElement elem(name, 0, offset, false, 1, row_major, matrixCount, fmt, hex);

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
        FormatElement elem(QString("%1[%2]").arg(name).arg(a), 0, offset, false, 1, row_major,
                           matrixCount, fmt, hex);

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
    fmt.compType = eCompType_UInt;
    fmt.compByteWidth = 4;
    fmt.compCount = 4;

    if(maxLen > 0 && maxLen < 16)
      fmt.compCount = 1;
    if(maxLen > 0 && maxLen < 4)
      fmt.compByteWidth = 1;

    elems.push_back(FormatElement("data", 0, 0, false, 1, false, 1, fmt, true));
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

  if(format.special && format.specialFormat == eSpecial_R5G5B5A1)
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
  else if(format.special && format.specialFormat == eSpecial_R5G6B5)
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
  else if(format.special && format.specialFormat == eSpecial_R4G4B4A4)
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
  else if(format.special && format.specialFormat == eSpecial_R10G10B10A2)
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

      if(format.compType == eCompType_UInt)
      {
        ret.push_back(r);
        ret.push_back(g);
        ret.push_back(b);
        ret.push_back(a);
      }
      else if(format.compType == eCompType_UScaled)
      {
        ret.push_back((float)r);
        ret.push_back((float)g);
        ret.push_back((float)b);
        ret.push_back((float)a);
      }
      else if(format.compType == eCompType_SInt || format.compType == eCompType_SScaled)
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

        if(format.compType == eCompType_SInt)
        {
          ret.push_back(ir);
          ret.push_back(ig);
          ret.push_back(ib);
          ret.push_back(ia);
        }
        else if(format.compType == eCompType_SScaled)
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
  else if(format.special && format.specialFormat == eSpecial_R11G11B10)
  {
    uint32_t packed = readObj<uint32_t>(data, end, ok);

    uint32_t xMantissa = ((packed >> 0) & 0x3f);
    uint32_t xExponent = ((packed >> 6) & 0x1f);
    uint32_t yMantissa = ((packed >> 11) & 0x3f);
    uint32_t yExponent = ((packed >> 17) & 0x1f);
    uint32_t zMantissa = ((packed >> 22) & 0x1f);
    uint32_t zExponent = ((packed >> 27) & 0x1f);

    ret.push_back(((float)(xMantissa) / 64.0f) * qPow(2.0f, (float)xExponent - 15.0f));
    ret.push_back(((float)(yMantissa) / 32.0f) * qPow(2.0f, (float)yExponent - 15.0f));
    ret.push_back(((float)(zMantissa) / 32.0f) * qPow(2.0f, (float)zExponent - 15.0f));
  }
  else
  {
    int dim = (int)(qMax(matrixdim, 1U) * format.compCount);

    for(int i = 0; i < dim; i++)
    {
      if(format.compType == eCompType_Float)
      {
        if(format.compByteWidth == 8)
          ret.push_back(readObj<double>(data, end, ok));
        else if(format.compByteWidth == 4)
          ret.push_back(readObj<float>(data, end, ok));
        else if(format.compByteWidth == 2)
          ret.push_back(Maths_HalfToFloat(readObj<uint16_t>(data, end, ok)));
      }
      else if(format.compType == eCompType_SInt)
      {
        if(format.compByteWidth == 4)
          ret.push_back((int)readObj<int32_t>(data, end, ok));
        else if(format.compByteWidth == 2)
          ret.push_back((int)readObj<int16_t>(data, end, ok));
        else if(format.compByteWidth == 1)
          ret.push_back((int)readObj<int8_t>(data, end, ok));
      }
      else if(format.compType == eCompType_UInt)
      {
        if(format.compByteWidth == 4)
          ret.push_back((uint32_t)readObj<uint32_t>(data, end, ok));
        else if(format.compByteWidth == 2)
          ret.push_back((uint32_t)readObj<uint16_t>(data, end, ok));
        else if(format.compByteWidth == 1)
          ret.push_back((uint32_t)readObj<uint8_t>(data, end, ok));
      }
      else if(format.compType == eCompType_UScaled)
      {
        if(format.compByteWidth == 4)
          ret.push_back((float)readObj<uint32_t>(data, end, ok));
        else if(format.compByteWidth == 2)
          ret.push_back((float)readObj<uint16_t>(data, end, ok));
        else if(format.compByteWidth == 1)
          ret.push_back((float)readObj<uint8_t>(data, end, ok));
      }
      else if(format.compType == eCompType_SScaled)
      {
        if(format.compByteWidth == 4)
          ret.push_back((float)readObj<int32_t>(data, end, ok));
        else if(format.compByteWidth == 2)
          ret.push_back((float)readObj<int16_t>(data, end, ok));
        else if(format.compByteWidth == 1)
          ret.push_back((float)readObj<int8_t>(data, end, ok));
      }
      else if(format.compType == eCompType_Depth)
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
      else if(format.compType == eCompType_Double)
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
  ret.type = eVar_Float;
  if(format.compType == eCompType_UInt)
    ret.type = eVar_UInt;
  if(format.compType == eCompType_SInt)
    ret.type = eVar_Int;
  if(format.compType == eCompType_Double)
    ret.type = eVar_Double;

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

      if(ret.type == eVar_Double)
        ret.value.dv[dst] = o.toDouble();
      else if(ret.type == eVar_UInt)
        ret.value.uv[dst] = o.toUInt();
      else if(ret.type == eVar_Int)
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
    if(format.specialFormat == eSpecial_R5G5B5A1 || format.specialFormat == eSpecial_R5G6B5 ||
       format.specialFormat == eSpecial_R4G4B4A4)
      vecSize = 2;

    if(format.specialFormat == eSpecial_R10G10B10A2 || format.specialFormat == eSpecial_R11G11B10)
      vecSize = 4;
  }

  return vecSize * matrixdim;
}

QString TypeString(const ShaderVariable &v)
{
  if(v.members.count > 0)
  {
    if(v.isStruct)
      return "struct";
    else
      return QString("%1[%2]").arg(TypeString(v.members[0]), v.members.count);
  }

  QString typeStr = ToQStr(v.type);

  if(v.displayAsHex && v.type == eVar_UInt)
    typeStr = "xint";

  if(v.rows == 1 && v.columns == 1)
    return typeStr;
  if(v.rows == 1)
    return QString("%1%2").arg(typeStr).arg(v.columns);
  else
    return QString("%1%2x%3").arg(typeStr).arg(v.rows).arg(v.columns);
}

template <typename el>
static QString RowValuesToString(int cols, el x, el y, el z, el w)
{
  if(cols == 1)
    return Formatter::Format(x);
  else if(cols == 2)
    return Formatter::Format(x) + ", " + Formatter::Format(y);
  else if(cols == 3)
    return Formatter::Format(x) + ", " + Formatter::Format(y) + ", " + Formatter::Format(z);
  else
    return Formatter::Format(x) + ", " + Formatter::Format(y) + ", " + Formatter::Format(z) + ", " +
           Formatter::Format(w);
}

QString RowString(const ShaderVariable &v, uint32_t row, VarType type)
{
  if(type == eVar_Unknown)
    type = v.type;

  if(type == eVar_Double)
    return RowValuesToString((int)v.columns, v.value.dv[row * v.columns + 0],
                             v.value.dv[row * v.columns + 1], v.value.dv[row * v.columns + 2],
                             v.value.dv[row * v.columns + 3]);
  else if(type == eVar_Int)
    return RowValuesToString((int)v.columns, v.value.iv[row * v.columns + 0],
                             v.value.iv[row * v.columns + 1], v.value.iv[row * v.columns + 2],
                             v.value.iv[row * v.columns + 3]);
  else if(type == eVar_UInt)
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
    return "";

  if(v.rows == 1)
    return RowString(v, 0);

  QString ret = "";
  for(int i = 0; i < (int)v.rows; i++)
  {
    if(i > 0)
      ret += ", ";
    ret += "{" + RowString(v, i) + "}";
  }

  return "{ " + ret + " }";
}

QString RowTypeString(const ShaderVariable &v)
{
  if(v.members.count > 0)
  {
    if(v.isStruct)
      return "struct";
    else
      return "flibbertygibbet";
  }

  if(v.rows == 0 && v.columns == 0)
    return "-";

  QString typeStr = ToQStr(v.type);

  if(v.displayAsHex && v.type == eVar_UInt)
    typeStr = "xint";

  if(v.columns == 1)
    return typeStr;

  return QString("%1%2").arg(typeStr).arg(v.columns);
}
