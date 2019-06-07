/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "core/core.h"
#include "strings/string_utils.h"
#include "gl_common.h"
#include "gl_driver.h"

struct ProgramUniformValue
{
  ProgramUniformValue()
  {
    Type = eGL_NONE;
    Location = 0;
    RDCEraseEl(data);
  }
  GLenum Type;
  int32_t Location;

  union
  {
    double dval[16];
    float fval[16];
    int32_t ival[16];
    uint32_t uval[16];
  } data;
};

DECLARE_REFLECTION_STRUCT(ProgramUniformValue);

struct ProgramUniform
{
  std::string Basename;
  bool IsArray = false;

  std::vector<ProgramUniformValue> Values;
};

DECLARE_REFLECTION_STRUCT(ProgramUniform);

struct ProgramBinding
{
  ProgramBinding() = default;
  ProgramBinding(const char *n, int32_t b) : Name(n), Binding(b) {}
  std::string Name;
  int32_t Binding = -1;
};

DECLARE_REFLECTION_STRUCT(ProgramBinding);

struct ProgramUniforms
{
  std::vector<ProgramUniform> ValueUniforms;
  std::vector<ProgramBinding> UBOBindings;
  std::vector<ProgramBinding> SSBOBindings;
};

DECLARE_REFLECTION_STRUCT(ProgramUniforms);

struct UnrolledSPIRVConstant
{
  GLenum glType = eGL_NONE;
  char name[1024] = {};
  uint32_t arraySize = 1;
  int32_t location = -1;
};

static GLenum MakeGLType(const ShaderVariableType &type)
{
  if(type.descriptor.type == VarType::Double)
  {
    if(type.descriptor.columns == 4 && type.descriptor.rows == 4)
      return eGL_DOUBLE_MAT4;
    if(type.descriptor.columns == 4 && type.descriptor.rows == 3)
      return eGL_DOUBLE_MAT4x3;
    if(type.descriptor.columns == 4 && type.descriptor.rows == 2)
      return eGL_DOUBLE_MAT4x2;
    if(type.descriptor.columns == 4 && type.descriptor.rows == 1)
      return eGL_DOUBLE_VEC4;

    if(type.descriptor.columns == 3 && type.descriptor.rows == 4)
      return eGL_DOUBLE_MAT3x4;
    if(type.descriptor.columns == 3 && type.descriptor.rows == 3)
      return eGL_DOUBLE_MAT3;
    if(type.descriptor.columns == 3 && type.descriptor.rows == 2)
      return eGL_DOUBLE_MAT3x2;
    if(type.descriptor.columns == 3 && type.descriptor.rows == 1)
      return eGL_DOUBLE_VEC3;

    if(type.descriptor.columns == 2 && type.descriptor.rows == 4)
      return eGL_DOUBLE_MAT2x4;
    if(type.descriptor.columns == 2 && type.descriptor.rows == 3)
      return eGL_DOUBLE_MAT2x4;
    if(type.descriptor.columns == 2 && type.descriptor.rows == 2)
      return eGL_DOUBLE_MAT2;
    if(type.descriptor.columns == 2 && type.descriptor.rows == 1)
      return eGL_DOUBLE_VEC2;

    if(type.descriptor.columns == 1 && type.descriptor.rows == 4)
      return eGL_DOUBLE_VEC4;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 3)
      return eGL_DOUBLE_VEC3;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 2)
      return eGL_DOUBLE_VEC2;

    if(type.descriptor.rows == 1 && type.descriptor.columns == 4)
      return eGL_DOUBLE_VEC4;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 3)
      return eGL_DOUBLE_VEC3;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 2)
      return eGL_DOUBLE_VEC2;

    return eGL_DOUBLE;
  }
  else if(type.descriptor.type == VarType::Float)
  {
    if(type.descriptor.columns == 4 && type.descriptor.rows == 4)
      return eGL_FLOAT_MAT4;
    if(type.descriptor.columns == 4 && type.descriptor.rows == 3)
      return eGL_FLOAT_MAT4x3;
    if(type.descriptor.columns == 4 && type.descriptor.rows == 2)
      return eGL_FLOAT_MAT4x2;
    if(type.descriptor.columns == 4 && type.descriptor.rows == 1)
      return eGL_FLOAT_VEC4;

    if(type.descriptor.columns == 3 && type.descriptor.rows == 4)
      return eGL_FLOAT_MAT3x4;
    if(type.descriptor.columns == 3 && type.descriptor.rows == 3)
      return eGL_FLOAT_MAT3;
    if(type.descriptor.columns == 3 && type.descriptor.rows == 2)
      return eGL_FLOAT_MAT3x2;
    if(type.descriptor.columns == 3 && type.descriptor.rows == 1)
      return eGL_FLOAT_VEC3;

    if(type.descriptor.columns == 2 && type.descriptor.rows == 4)
      return eGL_FLOAT_MAT2x4;
    if(type.descriptor.columns == 2 && type.descriptor.rows == 3)
      return eGL_FLOAT_MAT2x4;
    if(type.descriptor.columns == 2 && type.descriptor.rows == 2)
      return eGL_FLOAT_MAT2;
    if(type.descriptor.columns == 2 && type.descriptor.rows == 1)
      return eGL_FLOAT_VEC2;

    if(type.descriptor.columns == 1 && type.descriptor.rows == 4)
      return eGL_FLOAT_VEC4;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 3)
      return eGL_FLOAT_VEC3;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 2)
      return eGL_FLOAT_VEC2;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 1)
      return eGL_FLOAT;

    if(type.descriptor.rows == 1 && type.descriptor.columns == 4)
      return eGL_FLOAT_VEC4;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 3)
      return eGL_FLOAT_VEC3;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 2)
      return eGL_FLOAT_VEC2;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 1)
      return eGL_FLOAT;

    return eGL_FLOAT;
  }
  else if(type.descriptor.type == VarType::SInt)
  {
    if(type.descriptor.columns == 1 && type.descriptor.rows == 4)
      return eGL_INT_VEC4;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 3)
      return eGL_INT_VEC3;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 2)
      return eGL_INT_VEC2;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 1)
      return eGL_INT;

    if(type.descriptor.rows == 1 && type.descriptor.columns == 4)
      return eGL_INT_VEC4;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 3)
      return eGL_INT_VEC3;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 2)
      return eGL_INT_VEC2;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 1)
      return eGL_INT;

    return eGL_INT;
  }
  else if(type.descriptor.type == VarType::UInt)
  {
    if(type.descriptor.columns == 1 && type.descriptor.rows == 4)
      return eGL_UNSIGNED_INT_VEC4;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 3)
      return eGL_UNSIGNED_INT_VEC3;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 2)
      return eGL_UNSIGNED_INT_VEC2;
    if(type.descriptor.columns == 1 && type.descriptor.rows == 1)
      return eGL_UNSIGNED_INT;

    if(type.descriptor.rows == 1 && type.descriptor.columns == 4)
      return eGL_UNSIGNED_INT_VEC4;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 3)
      return eGL_UNSIGNED_INT_VEC3;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 2)
      return eGL_UNSIGNED_INT_VEC2;
    if(type.descriptor.rows == 1 && type.descriptor.columns == 1)
      return eGL_UNSIGNED_INT;

    return eGL_UNSIGNED_INT;
  }

  RDCERR("Unhandled GL type");

  return eGL_FLOAT;
}

static GLenum MakeGLType(const ShaderResource &res)
{
  if(res.variableType.descriptor.type == VarType::UInt)
  {
    switch(res.resType)
    {
      case TextureType::Buffer: return eGL_UNSIGNED_INT_SAMPLER_BUFFER;
      case TextureType::Texture1D: return eGL_UNSIGNED_INT_SAMPLER_1D;
      case TextureType::Texture1DArray: return eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY;
      case TextureType::Texture2D: return eGL_UNSIGNED_INT_SAMPLER_2D;
      case TextureType::TextureRect: return eGL_UNSIGNED_INT_SAMPLER_2D_RECT;
      case TextureType::Texture2DArray: return eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY;
      case TextureType::Texture2DMS: return eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE;
      case TextureType::Texture2DMSArray: return eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY;
      case TextureType::Texture3D: return eGL_UNSIGNED_INT_SAMPLER_3D;
      case TextureType::TextureCube: return eGL_UNSIGNED_INT_SAMPLER_CUBE;
      case TextureType::TextureCubeArray: return eGL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY;
      default: break;
    }
  }
  else if(res.variableType.descriptor.type == VarType::SInt)
  {
    switch(res.resType)
    {
      case TextureType::Buffer: return eGL_INT_SAMPLER_BUFFER;
      case TextureType::Texture1D: return eGL_INT_SAMPLER_1D;
      case TextureType::Texture1DArray: return eGL_INT_SAMPLER_1D_ARRAY;
      case TextureType::Texture2D: return eGL_INT_SAMPLER_2D;
      case TextureType::TextureRect: return eGL_INT_SAMPLER_2D_RECT;
      case TextureType::Texture2DArray: return eGL_INT_SAMPLER_2D_ARRAY;
      case TextureType::Texture2DMS: return eGL_INT_SAMPLER_2D_MULTISAMPLE;
      case TextureType::Texture2DMSArray: return eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY;
      case TextureType::Texture3D: return eGL_INT_SAMPLER_3D;
      case TextureType::TextureCube: return eGL_INT_SAMPLER_CUBE;
      case TextureType::TextureCubeArray: return eGL_INT_SAMPLER_CUBE_MAP_ARRAY;
      default: break;
    }
  }
  else
  {
    switch(res.resType)
    {
      case TextureType::Buffer: return eGL_SAMPLER_BUFFER;
      case TextureType::Texture1D: return eGL_SAMPLER_1D;
      case TextureType::Texture1DArray: return eGL_SAMPLER_1D_ARRAY;
      case TextureType::Texture2D: return eGL_SAMPLER_2D;
      case TextureType::TextureRect: return eGL_SAMPLER_2D_RECT;
      case TextureType::Texture2DArray: return eGL_SAMPLER_2D_ARRAY;
      case TextureType::Texture2DMS: return eGL_SAMPLER_2D_MULTISAMPLE;
      case TextureType::Texture2DMSArray: return eGL_SAMPLER_2D_MULTISAMPLE_ARRAY;
      case TextureType::Texture3D: return eGL_SAMPLER_3D;
      case TextureType::TextureCube: return eGL_SAMPLER_CUBE;
      case TextureType::TextureCubeArray: return eGL_SAMPLER_CUBE_MAP_ARRAY;
      default: break;
    }
  }

  RDCERR("Unhandled GL type");

  return eGL_SAMPLER_2D;
}

static void UnrollConstant(rdcarray<UnrolledSPIRVConstant> &unrolled, const ShaderConstant &var,
                           const rdcstr &basename, uint32_t &location)
{
  rdcstr name = basename;

  if(!basename.empty())
  {
    if(var.name[0] == '[')
      name += var.name;
    else
      name += "." + var.name;
  }
  else
  {
    name = var.name;
  }

  const uint32_t arraySize = RDCMAX(1U, var.type.descriptor.elements);

  if(var.type.members.empty())
  {
    if(arraySize > 1)
      name += "[0]";

    UnrolledSPIRVConstant u;
    u.glType = MakeGLType(var.type);
    memcpy(u.name, name.c_str(), RDCMIN(name.size(), ARRAY_COUNT(u.name)));
    u.arraySize = arraySize;
    u.location = basename.empty() ? var.byteOffset : location;

    unrolled.push_back(u);

    location += arraySize;
  }
  else
  {
    if(basename.empty())
      location = var.byteOffset;

    for(uint32_t i = 0; i < arraySize; i++)
    {
      if(arraySize > 1)
        name += StringFormat::Fmt("[%u]", i);

      for(const ShaderConstant &member : var.type.members)
        UnrollConstant(unrolled, member, name, location);
    }
  }
}

static void UnrollConstant(rdcarray<UnrolledSPIRVConstant> &unrolled, const ShaderConstant &var)
{
  uint32_t location;
  UnrollConstant(unrolled, var, rdcstr(), location);
}

static void UnrollConstants(const PerStageReflections &stages,
                            rdcarray<UnrolledSPIRVConstant> &globals)
{
  for(size_t s = 0; s < 6; s++)
  {
    if(!stages.refls[s])
      continue;

    // check if we have a non-buffer backed UBO, if so that's our globals.
    for(const ConstantBlock &cb : stages.refls[s]->constantBlocks)
    {
      if(!cb.bufferBacked && cb.byteSize > 0)
      {
        for(const ShaderConstant &shaderConst : cb.variables)
        {
          // location is stored in the byteOffset. Search to see if we already have a global at
          // this location since stages can share the same globals
          int32_t location = (int32_t)shaderConst.byteOffset;

          bool already = false;

          for(const UnrolledSPIRVConstant &existing : globals)
          {
            if(existing.location == location)
            {
              already = true;
              break;
            }
          }

          if(!already)
            UnrollConstant(globals, shaderConst);
        }
      }
    }

    // now include the samplers which can be bound
    for(const ShaderResource &res : stages.refls[s]->readOnlyResources)
    {
      if(res.isTexture && res.bindPoint < stages.mappings[s]->readOnlyResources.count())
      {
        int32_t location = -stages.mappings[s]->readOnlyResources[res.bindPoint].bind;

        bool already = false;

        for(const UnrolledSPIRVConstant &existing : globals)
        {
          if(existing.location == location)
          {
            already = true;
            break;
          }
        }

        if(!already)
        {
          UnrolledSPIRVConstant u;
          u.glType = MakeGLType(res);
          memcpy(u.name, res.name.c_str(), RDCMIN(res.name.size(), ARRAY_COUNT(u.name)));
          u.arraySize = 1;
          u.location = location;
          globals.push_back(u);
        }
      }
    }
  }
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ProgramUniformValue &el)
{
  SERIALISE_MEMBER(Type);
  SERIALISE_MEMBER(Location);

  // some special logic here, we decode Type to figure out what the actual data is, and serialise it
  // with the right type.

  VarType baseType = VarType::Float;
  uint32_t elemCount = 1;

  switch(el.Type)
  {
    case eGL_FLOAT_MAT4:
    case eGL_FLOAT_MAT4x3:
    case eGL_FLOAT_MAT4x2:
    case eGL_FLOAT_MAT3:
    case eGL_FLOAT_MAT3x4:
    case eGL_FLOAT_MAT3x2:
    case eGL_FLOAT_MAT2:
    case eGL_FLOAT_MAT2x4:
    case eGL_FLOAT_MAT2x3:
    case eGL_FLOAT:
    case eGL_FLOAT_VEC2:
    case eGL_FLOAT_VEC3:
    case eGL_FLOAT_VEC4: baseType = VarType::Float; break;
    case eGL_DOUBLE_MAT4:
    case eGL_DOUBLE_MAT4x3:
    case eGL_DOUBLE_MAT4x2:
    case eGL_DOUBLE_MAT3:
    case eGL_DOUBLE_MAT3x4:
    case eGL_DOUBLE_MAT3x2:
    case eGL_DOUBLE_MAT2:
    case eGL_DOUBLE_MAT2x4:
    case eGL_DOUBLE_MAT2x3:
    case eGL_DOUBLE:
    case eGL_DOUBLE_VEC2:
    case eGL_DOUBLE_VEC3:
    case eGL_DOUBLE_VEC4: baseType = VarType::Double; break;
    case eGL_SAMPLER_1D:
    case eGL_SAMPLER_2D:
    case eGL_SAMPLER_3D:
    case eGL_SAMPLER_CUBE:
    case eGL_SAMPLER_CUBE_MAP_ARRAY:
    case eGL_SAMPLER_1D_SHADOW:
    case eGL_SAMPLER_2D_SHADOW:
    case eGL_SAMPLER_1D_ARRAY:
    case eGL_SAMPLER_2D_ARRAY:
    case eGL_SAMPLER_1D_ARRAY_SHADOW:
    case eGL_SAMPLER_2D_ARRAY_SHADOW:
    case eGL_SAMPLER_2D_MULTISAMPLE:
    case eGL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case eGL_SAMPLER_CUBE_SHADOW:
    case eGL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
    case eGL_SAMPLER_BUFFER:
    case eGL_SAMPLER_2D_RECT:
    case eGL_SAMPLER_2D_RECT_SHADOW:
    case eGL_INT_SAMPLER_1D:
    case eGL_INT_SAMPLER_2D:
    case eGL_INT_SAMPLER_3D:
    case eGL_INT_SAMPLER_CUBE:
    case eGL_INT_SAMPLER_CUBE_MAP_ARRAY:
    case eGL_INT_SAMPLER_1D_ARRAY:
    case eGL_INT_SAMPLER_2D_ARRAY:
    case eGL_INT_SAMPLER_2D_MULTISAMPLE:
    case eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case eGL_INT_SAMPLER_BUFFER:
    case eGL_INT_SAMPLER_2D_RECT:
    case eGL_UNSIGNED_INT_SAMPLER_1D:
    case eGL_UNSIGNED_INT_SAMPLER_2D:
    case eGL_UNSIGNED_INT_SAMPLER_3D:
    case eGL_UNSIGNED_INT_SAMPLER_CUBE:
    case eGL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
    case eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case eGL_UNSIGNED_INT_SAMPLER_BUFFER:
    case eGL_UNSIGNED_INT_SAMPLER_2D_RECT:
    case eGL_IMAGE_1D:
    case eGL_IMAGE_2D:
    case eGL_IMAGE_3D:
    case eGL_IMAGE_2D_RECT:
    case eGL_IMAGE_CUBE:
    case eGL_IMAGE_BUFFER:
    case eGL_IMAGE_1D_ARRAY:
    case eGL_IMAGE_2D_ARRAY:
    case eGL_IMAGE_CUBE_MAP_ARRAY:
    case eGL_IMAGE_2D_MULTISAMPLE:
    case eGL_IMAGE_2D_MULTISAMPLE_ARRAY:
    case eGL_INT_IMAGE_1D:
    case eGL_INT_IMAGE_2D:
    case eGL_INT_IMAGE_3D:
    case eGL_INT_IMAGE_2D_RECT:
    case eGL_INT_IMAGE_CUBE:
    case eGL_INT_IMAGE_BUFFER:
    case eGL_INT_IMAGE_1D_ARRAY:
    case eGL_INT_IMAGE_2D_ARRAY:
    case eGL_INT_IMAGE_2D_MULTISAMPLE:
    case eGL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
    case eGL_UNSIGNED_INT_IMAGE_1D:
    case eGL_UNSIGNED_INT_IMAGE_2D:
    case eGL_UNSIGNED_INT_IMAGE_3D:
    case eGL_UNSIGNED_INT_IMAGE_2D_RECT:
    case eGL_UNSIGNED_INT_IMAGE_CUBE:
    case eGL_UNSIGNED_INT_IMAGE_BUFFER:
    case eGL_UNSIGNED_INT_IMAGE_1D_ARRAY:
    case eGL_UNSIGNED_INT_IMAGE_2D_ARRAY:
    case eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
    case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
    case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
    case eGL_UNSIGNED_INT_ATOMIC_COUNTER:
    case eGL_INT:
    case eGL_INT_VEC2:
    case eGL_INT_VEC3:
    case eGL_INT_VEC4: baseType = VarType::SInt; break;
    case eGL_UNSIGNED_INT:
    case eGL_BOOL:
    case eGL_UNSIGNED_INT_VEC2:
    case eGL_BOOL_VEC2:
    case eGL_UNSIGNED_INT_VEC3:
    case eGL_BOOL_VEC3:
    case eGL_UNSIGNED_INT_VEC4:
    case eGL_BOOL_VEC4: baseType = VarType::UInt; break;
    default:
      RDCERR("Unhandled uniform type '%s'", ToStr(el.Type).c_str());
      baseType = VarType::Float;
      elemCount = 1;
      break;
  }

  switch(el.Type)
  {
    case eGL_FLOAT_MAT4:
    case eGL_DOUBLE_MAT4: elemCount = 16; break;
    case eGL_FLOAT_MAT4x3:
    case eGL_FLOAT_MAT3x4:
    case eGL_DOUBLE_MAT4x3:
    case eGL_DOUBLE_MAT3x4: elemCount = 12; break;
    case eGL_FLOAT_MAT4x2:
    case eGL_FLOAT_MAT2x4:
    case eGL_DOUBLE_MAT4x2:
    case eGL_DOUBLE_MAT2x4: elemCount = 8; break;
    case eGL_FLOAT_MAT3:
    case eGL_DOUBLE_MAT3: elemCount = 9; break;
    case eGL_FLOAT_MAT3x2:
    case eGL_DOUBLE_MAT3x2:
    case eGL_FLOAT_MAT2x3:
    case eGL_DOUBLE_MAT2x3: elemCount = 6; break;
    case eGL_FLOAT_MAT2:
    case eGL_DOUBLE_MAT2:
    case eGL_FLOAT_VEC4:
    case eGL_DOUBLE_VEC4: elemCount = 4; break;
    case eGL_FLOAT_VEC3:
    case eGL_DOUBLE_VEC3: elemCount = 3; break;
    case eGL_FLOAT_VEC2:
    case eGL_DOUBLE_VEC2: elemCount = 2; break;
    default:
      // all other types are elemCount = 1
      break;
  }

  double *dv = el.data.dval;
  float *fv = el.data.fval;
  int32_t *iv = el.data.ival;
  uint32_t *uv = el.data.uval;

  // originally the logic was backwards and floats were serialised with dv and doubles with fv.
  // This caused extra garbage to be written for floats, and truncated double data.
  if(ser.VersionAtLeast(0x1C))
  {
    if(baseType == VarType::Float)
      ser.Serialise("data"_lit, fv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::SInt)
      ser.Serialise("data"_lit, iv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::UInt)
      ser.Serialise("data"_lit, uv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::Double)
      ser.Serialise("data"_lit, dv, elemCount, SerialiserFlags::NoFlags);
  }
  else
  {
    if(baseType == VarType::Double)
      ser.Serialise("data"_lit, fv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::Float)
      ser.Serialise("data"_lit, dv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::SInt)
      ser.Serialise("data"_lit, iv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::UInt)
      ser.Serialise("data"_lit, uv, elemCount, SerialiserFlags::NoFlags);
  }
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ProgramUniform &el)
{
  SERIALISE_MEMBER(Basename);
  SERIALISE_MEMBER(IsArray);
  SERIALISE_MEMBER(Values);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ProgramBinding &el)
{
  SERIALISE_MEMBER(Name);
  SERIALISE_MEMBER(Binding);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ProgramUniforms &el)
{
  SERIALISE_MEMBER(ValueUniforms);
  SERIALISE_MEMBER(UBOBindings);
  SERIALISE_MEMBER(SSBOBindings);
}

template <const bool CopyUniforms, const bool SerialiseUniforms, typename SerialiserType>
static void ForAllProgramUniforms(SerialiserType *ser, CaptureState state,
                                  const PerStageReflections &srcStages, GLuint progSrc,
                                  const PerStageReflections &dstStages, GLuint progDst,
                                  std::map<GLint, GLint> *locTranslate)
{
  const bool ReadSourceProgram = CopyUniforms || (SerialiseUniforms && ser && ser->IsWriting());
  const bool WriteDestProgram = CopyUniforms || (SerialiseUniforms && ser && ser->IsReading());

  RDCCOMPILE_ASSERT((CopyUniforms && !SerialiseUniforms) || (!CopyUniforms && SerialiseUniforms),
                    "Invalid call to ForAllProgramUniforms");

  // When programs are SPIR-V we have to rely on our own reflection since the driver's reflection
  // can't be trusted, or at least used in a normal way. Since SPIR-V is immutable for many things
  // we only need to process uniform values - for compatibility we still serialise the same, but we
  // skip fetching or applying UBO bindings etc.
  bool IsSrcProgramSPIRV = false;
  for(size_t i = 0; i < 6; i++)
    IsSrcProgramSPIRV |= srcStages.refls[i] && srcStages.refls[i]->encoding == ShaderEncoding::SPIRV;

  bool IsDstProgramSPIRV = false;
  for(size_t i = 0; i < 6; i++)
    IsDstProgramSPIRV |= dstStages.refls[i] && dstStages.refls[i]->encoding == ShaderEncoding::SPIRV;

  RDCASSERTMSG("Expect both programs to be SPIR-V in ForAllProgramUniforms",
               IsSrcProgramSPIRV == IsDstProgramSPIRV, IsSrcProgramSPIRV, IsDstProgramSPIRV);

  // this struct will be serialised with the uniform binding data, or if we're just copying it will
  // be used to store the data fetched from the source program, before being applied to the
  // destination program. It's slightly redundant since we could unify the loops (as the code used
  // to do) but it's much better for code organisation and clarity to have a single path whether
  // serialising or not.
  ProgramUniforms serialisedUniforms;

  // if we're reading the source program, iterate over the interfaces and fetch the data.
  if(CheckConstParam(ReadSourceProgram))
  {
    constexpr size_t numProps = 5;
    constexpr GLenum resProps[numProps] = {
        eGL_BLOCK_INDEX, eGL_TYPE, eGL_NAME_LENGTH, eGL_ARRAY_SIZE, eGL_LOCATION,
    };
    GLint values[numProps];

    GLint NumUniforms = 0;
    rdcarray<UnrolledSPIRVConstant> spirvGlobals;

    if(IsSrcProgramSPIRV)
    {
      // Unfortunately since this is a program-global reflection we need to go through each shader
      // and add its variables (if they don't already exist) to get a union of all shaders for the
      // program.
      UnrollConstants(srcStages, spirvGlobals);

      NumUniforms = (GLint)spirvGlobals.size();
    }
    else
    {
      GL.glGetProgramInterfaceiv(progSrc, eGL_UNIFORM, eGL_ACTIVE_RESOURCES, &NumUniforms);
    }

    // this is a very conservative figure - many uniforms will be in UBOs and so will be ignored
    serialisedUniforms.ValueUniforms.reserve(NumUniforms);

    for(GLint i = 0; i < NumUniforms; i++)
    {
      GLenum type = eGL_NONE;
      int32_t arraySize = 0;
      int32_t srcLocation = 0;
      std::string basename;
      bool isArray = false;

      if(IsSrcProgramSPIRV)
      {
        // hardcode manual reflection from SPIR-V constant.
        RDCCOMPILE_ASSERT(numProps == 5 && resProps[0] == eGL_BLOCK_INDEX &&
                              resProps[1] == eGL_TYPE && resProps[2] == eGL_NAME_LENGTH &&
                              resProps[3] == eGL_ARRAY_SIZE && resProps[4] == eGL_LOCATION,
                          "reflection properties have changed - update manual SPIR-V reflection");

        // these are implicitly globals
        values[0] = -1;
        values[1] = spirvGlobals[i].glType;
        values[2] = 1;    // unused
        values[3] = spirvGlobals[i].arraySize;
        values[4] = spirvGlobals[i].location;
      }
      else
      {
        GL.glGetProgramResourceiv(progSrc, eGL_UNIFORM, i, numProps, resProps, numProps, NULL,
                                  values);
      }

      // we don't need to consider uniforms within UBOs
      if(values[0] >= 0)
        continue;

      // get the metadata we need for fetching the data
      type = (GLenum)values[1];
      arraySize = values[3];
      srcLocation = values[4];

      char n[1024] = {0};
      if(IsSrcProgramSPIRV)
      {
        RDCCOMPILE_ASSERT(sizeof(n) == sizeof(spirvGlobals[i].name), "Array sizes have changed");
        memcpy(n, spirvGlobals[i].name, sizeof(n));
      }
      else
      {
        GL.glGetProgramResourceName(progSrc, eGL_UNIFORM, i, values[2], NULL, n);
      }

      if(arraySize > 1)
      {
        isArray = true;

        size_t len = strlen(n);

        if(n[len - 3] == '[' && n[len - 2] == '0' && n[len - 1] == ']')
          n[len - 3] = 0;
      }
      else
      {
        arraySize = 1;
      }

      basename = n;

      // push it onto the list
      serialisedUniforms.ValueUniforms.push_back(ProgramUniform());
      ProgramUniform &uniform = serialisedUniforms.ValueUniforms.back();

      uniform.Basename = basename;
      uniform.IsArray = isArray;
      uniform.Values.resize(arraySize);

      GLuint baseLocation = srcLocation;

      // loop over every element in the array (arraySize = 1 for non arrays)
      for(GLint arr = 0; arr < arraySize; arr++)
      {
        ProgramUniformValue &uniformVal = uniform.Values[arr];
        uniformVal.Type = type;
        uniformVal.Location = srcLocation;

        std::string name = basename;

        // atomic counters cannot be changed, don't fetch their value
        if(type == eGL_UNSIGNED_INT_ATOMIC_COUNTER)
          continue;

        if(srcLocation == -1)
          RDCWARN("Couldn't get srcLocation for %s", name.c_str());

        // append the subscript if this item is an array.
        if(isArray)
        {
          name += StringFormat::Fmt("[%d]", arr);

          if(IsSrcProgramSPIRV)
            uniformVal.Location = srcLocation = baseLocation + arr;
          else
            uniformVal.Location = srcLocation = GL.glGetUniformLocation(progSrc, name.c_str());

          if(srcLocation == -1)
            RDCWARN("Couldn't get srcLocation for %s", name.c_str());
        }

        // fetch the data into the ProgramUniformValue, with the appropriate method for its type
        double *dv = uniformVal.data.dval;
        float *fv = uniformVal.data.fval;
        int32_t *iv = uniformVal.data.ival;
        uint32_t *uiv = uniformVal.data.uval;

        switch(type)
        {
          case eGL_FLOAT_MAT4:
          case eGL_FLOAT_MAT4x3:
          case eGL_FLOAT_MAT4x2:
          case eGL_FLOAT_MAT3:
          case eGL_FLOAT_MAT3x4:
          case eGL_FLOAT_MAT3x2:
          case eGL_FLOAT_MAT2:
          case eGL_FLOAT_MAT2x4:
          case eGL_FLOAT_MAT2x3:
          case eGL_FLOAT:
          case eGL_FLOAT_VEC2:
          case eGL_FLOAT_VEC3:
          case eGL_FLOAT_VEC4: GL.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_DOUBLE_MAT4:
          case eGL_DOUBLE_MAT4x3:
          case eGL_DOUBLE_MAT4x2:
          case eGL_DOUBLE_MAT3:
          case eGL_DOUBLE_MAT3x4:
          case eGL_DOUBLE_MAT3x2:
          case eGL_DOUBLE_MAT2:
          case eGL_DOUBLE_MAT2x4:
          case eGL_DOUBLE_MAT2x3:
          case eGL_DOUBLE:
          case eGL_DOUBLE_VEC2:
          case eGL_DOUBLE_VEC3:
          case eGL_DOUBLE_VEC4:
            GL.glGetUniformdv(progSrc, srcLocation, dv);
            break;

          // treat all samplers as just an int (since they just store their binding value)
          case eGL_SAMPLER_1D:
          case eGL_SAMPLER_2D:
          case eGL_SAMPLER_3D:
          case eGL_SAMPLER_CUBE:
          case eGL_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_SAMPLER_1D_SHADOW:
          case eGL_SAMPLER_2D_SHADOW:
          case eGL_SAMPLER_1D_ARRAY:
          case eGL_SAMPLER_2D_ARRAY:
          case eGL_SAMPLER_1D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_MULTISAMPLE:
          case eGL_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_SAMPLER_CUBE_SHADOW:
          case eGL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
          case eGL_SAMPLER_BUFFER:
          case eGL_SAMPLER_2D_RECT:
          case eGL_SAMPLER_2D_RECT_SHADOW:
          case eGL_INT_SAMPLER_1D:
          case eGL_INT_SAMPLER_2D:
          case eGL_INT_SAMPLER_3D:
          case eGL_INT_SAMPLER_CUBE:
          case eGL_INT_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_INT_SAMPLER_1D_ARRAY:
          case eGL_INT_SAMPLER_2D_ARRAY:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_SAMPLER_BUFFER:
          case eGL_INT_SAMPLER_2D_RECT:
          case eGL_UNSIGNED_INT_SAMPLER_1D:
          case eGL_UNSIGNED_INT_SAMPLER_2D:
          case eGL_UNSIGNED_INT_SAMPLER_3D:
          case eGL_UNSIGNED_INT_SAMPLER_CUBE:
          case eGL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_BUFFER:
          case eGL_UNSIGNED_INT_SAMPLER_2D_RECT:
          case eGL_IMAGE_1D:
          case eGL_IMAGE_2D:
          case eGL_IMAGE_3D:
          case eGL_IMAGE_2D_RECT:
          case eGL_IMAGE_CUBE:
          case eGL_IMAGE_BUFFER:
          case eGL_IMAGE_1D_ARRAY:
          case eGL_IMAGE_2D_ARRAY:
          case eGL_IMAGE_CUBE_MAP_ARRAY:
          case eGL_IMAGE_2D_MULTISAMPLE:
          case eGL_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_IMAGE_1D:
          case eGL_INT_IMAGE_2D:
          case eGL_INT_IMAGE_3D:
          case eGL_INT_IMAGE_2D_RECT:
          case eGL_INT_IMAGE_CUBE:
          case eGL_INT_IMAGE_BUFFER:
          case eGL_INT_IMAGE_1D_ARRAY:
          case eGL_INT_IMAGE_2D_ARRAY:
          case eGL_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_1D:
          case eGL_UNSIGNED_INT_IMAGE_2D:
          case eGL_UNSIGNED_INT_IMAGE_3D:
          case eGL_UNSIGNED_INT_IMAGE_2D_RECT:
          case eGL_UNSIGNED_INT_IMAGE_CUBE:
          case eGL_UNSIGNED_INT_IMAGE_BUFFER:
          case eGL_UNSIGNED_INT_IMAGE_1D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_INT:
          case eGL_INT_VEC2:
          case eGL_INT_VEC3:
          case eGL_INT_VEC4:
            GL.glGetUniformiv(progSrc, srcLocation, iv);
            break;
          // bools are unsigned integers
          case eGL_UNSIGNED_INT:
          case eGL_BOOL:
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_BOOL_VEC2:
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_BOOL_VEC3:
          case eGL_UNSIGNED_INT_VEC4:
          case eGL_BOOL_VEC4: GL.glGetUniformuiv(progSrc, srcLocation, uiv); break;
          default: RDCERR("Unhandled uniform type '%s'", ToStr(type).c_str());
        }
      }
    }

    // now find how many UBOs we have, and store their binding indices
    GLint numUBOs = 0;

    // SPIR-V shaders don't allow changing UBO values, so simply omit them entirely
    if(!IsSrcProgramSPIRV)
      GL.glGetProgramInterfaceiv(progSrc, eGL_UNIFORM_BLOCK, eGL_ACTIVE_RESOURCES, &numUBOs);

    serialisedUniforms.UBOBindings.reserve(numUBOs);

    for(GLint i = 0; i < numUBOs; i++)
    {
      GLenum prop = eGL_BUFFER_BINDING;
      uint32_t bind = 0;

      GL.glGetProgramResourceiv(progSrc, eGL_UNIFORM_BLOCK, i, 1, &prop, 1, NULL, (GLint *)&bind);

      char n[1024] = {0};
      GL.glGetProgramResourceName(progSrc, eGL_UNIFORM_BLOCK, i, 1023, NULL, n);

      serialisedUniforms.UBOBindings.push_back(ProgramBinding(n, bind));
    }

    // finally, if SSBOs are supported on this implementation, fetch their bindings
    GLint numSSBOs = 0;

    // SPIR-V shaders don't allow changing SSBO values, so simply omit them entirely
    if(HasExt[ARB_shader_storage_buffer_object] && !IsSrcProgramSPIRV)
      GL.glGetProgramInterfaceiv(progSrc, eGL_SHADER_STORAGE_BLOCK, eGL_ACTIVE_RESOURCES, &numSSBOs);

    serialisedUniforms.SSBOBindings.reserve(numSSBOs);

    for(GLint i = 0; i < numSSBOs; i++)
    {
      GLenum prop = eGL_BUFFER_BINDING;
      uint32_t bind = 0;

      GL.glGetProgramResourceiv(progSrc, eGL_SHADER_STORAGE_BLOCK, i, 1, &prop, 1, NULL,
                                (GLint *)&bind);

      char n[1024] = {0};
      GL.glGetProgramResourceName(progSrc, eGL_SHADER_STORAGE_BLOCK, i, 1023, NULL, n);

      serialisedUniforms.SSBOBindings.push_back(ProgramBinding(n, bind));
    }
  }

  // now serialise all the bindings if we are serialising
  if(CheckConstParam(SerialiseUniforms) && ser)
  {
    ser->Serialise("ProgramUniforms"_lit, serialisedUniforms);
  }

  // if we are writing to a destination program and replaying, then apply the stored data from
  // serialisedUniforms
  if(CheckConstParam(WriteDestProgram) && IsReplayMode(state))
  {
    rdcarray<UnrolledSPIRVConstant> spirvGlobals;
    if(IsDstProgramSPIRV)
      UnrollConstants(dstStages, spirvGlobals);

    // loop over the loose global uniforms, see if there is an equivalent, and apply it.
    for(const ProgramUniform &uniform : serialisedUniforms.ValueUniforms)
    {
      for(size_t arr = 0; arr < uniform.Values.size(); arr++)
      {
        const ProgramUniformValue &val = uniform.Values[arr];

        std::string name = uniform.Basename;

        if(uniform.IsArray)
          name += StringFormat::Fmt("[%u]", (uint32_t)arr);

        GLint dstLocation = -1;

        if(IsDstProgramSPIRV)
        {
          dstLocation = -1;

          int32_t baseLocation = val.Location - (int32_t)arr;

          RDCASSERT(baseLocation == uniform.Values[0].Location);

          // for SPIR-V the locations are fixed in the shader and are not mutable. We just check for
          // existance of something with this location. If nothing is found, we return -1
          // (non-existant) which prevents us from trying to write to a bad location.
          for(const UnrolledSPIRVConstant &var : spirvGlobals)
          {
            if(var.location == baseLocation)
            {
              dstLocation = val.Location;
              break;
            }
          }
        }
        else
        {
          dstLocation = GL.glGetUniformLocation(progDst, name.c_str());
        }

        if(locTranslate)
          (*locTranslate)[val.Location] = dstLocation;

        // don't try and apply the uniform if the new location is -1
        if(dstLocation == -1)
          continue;

        const double *dv = val.data.dval;
        const float *fv = val.data.fval;
        const int32_t *iv = val.data.ival;
        const uint32_t *uiv = val.data.uval;

        // call the appropriate function to apply the data to the destination program
        switch(val.Type)
        {
          case eGL_FLOAT_MAT4:
            GL.glProgramUniformMatrix4fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT4x3:
            GL.glProgramUniformMatrix4x3fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT4x2:
            GL.glProgramUniformMatrix4x2fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT3:
            GL.glProgramUniformMatrix3fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT3x4:
            GL.glProgramUniformMatrix3x4fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT3x2:
            GL.glProgramUniformMatrix3x2fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT2:
            GL.glProgramUniformMatrix2fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT2x4:
            GL.glProgramUniformMatrix2x4fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT2x3:
            GL.glProgramUniformMatrix2x3fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_DOUBLE_MAT4:
            GL.glProgramUniformMatrix4dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT4x3:
            GL.glProgramUniformMatrix4x3dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT4x2:
            GL.glProgramUniformMatrix4x2dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT3:
            GL.glProgramUniformMatrix3dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT3x4:
            GL.glProgramUniformMatrix3x4dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT3x2:
            GL.glProgramUniformMatrix3x2dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT2:
            GL.glProgramUniformMatrix2dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT2x4:
            GL.glProgramUniformMatrix2x4dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT2x3:
            GL.glProgramUniformMatrix2x3dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_FLOAT: GL.glProgramUniform1fv(progDst, dstLocation, 1, fv); break;
          case eGL_FLOAT_VEC2: GL.glProgramUniform2fv(progDst, dstLocation, 1, fv); break;
          case eGL_FLOAT_VEC3: GL.glProgramUniform3fv(progDst, dstLocation, 1, fv); break;
          case eGL_FLOAT_VEC4: GL.glProgramUniform4fv(progDst, dstLocation, 1, fv); break;
          case eGL_DOUBLE: GL.glProgramUniform1dv(progDst, dstLocation, 1, dv); break;
          case eGL_DOUBLE_VEC2: GL.glProgramUniform2dv(progDst, dstLocation, 1, dv); break;
          case eGL_DOUBLE_VEC3: GL.glProgramUniform3dv(progDst, dstLocation, 1, dv); break;
          case eGL_DOUBLE_VEC4: GL.glProgramUniform4dv(progDst, dstLocation, 1, dv); break;
          case eGL_INT_VEC2: GL.glProgramUniform2iv(progDst, dstLocation, 1, iv); break;
          case eGL_INT_VEC3: GL.glProgramUniform3iv(progDst, dstLocation, 1, iv); break;
          case eGL_INT_VEC4: GL.glProgramUniform4iv(progDst, dstLocation, 1, iv); break;
          case eGL_UNSIGNED_INT:
          case eGL_BOOL: GL.glProgramUniform1uiv(progDst, dstLocation, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_BOOL_VEC2: GL.glProgramUniform2uiv(progDst, dstLocation, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_BOOL_VEC3: GL.glProgramUniform3uiv(progDst, dstLocation, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC4:
          case eGL_BOOL_VEC4: GL.glProgramUniform4uiv(progDst, dstLocation, 1, uiv); break;

          case eGL_IMAGE_1D:
          case eGL_IMAGE_2D:
          case eGL_IMAGE_3D:
          case eGL_IMAGE_2D_RECT:
          case eGL_IMAGE_CUBE:
          case eGL_IMAGE_BUFFER:
          case eGL_IMAGE_1D_ARRAY:
          case eGL_IMAGE_2D_ARRAY:
          case eGL_IMAGE_CUBE_MAP_ARRAY:
          case eGL_IMAGE_2D_MULTISAMPLE:
          case eGL_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_IMAGE_1D:
          case eGL_INT_IMAGE_2D:
          case eGL_INT_IMAGE_3D:
          case eGL_INT_IMAGE_2D_RECT:
          case eGL_INT_IMAGE_CUBE:
          case eGL_INT_IMAGE_BUFFER:
          case eGL_INT_IMAGE_1D_ARRAY:
          case eGL_INT_IMAGE_2D_ARRAY:
          case eGL_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_1D:
          case eGL_UNSIGNED_INT_IMAGE_2D:
          case eGL_UNSIGNED_INT_IMAGE_3D:
          case eGL_UNSIGNED_INT_IMAGE_2D_RECT:
          case eGL_UNSIGNED_INT_IMAGE_CUBE:
          case eGL_UNSIGNED_INT_IMAGE_BUFFER:
          case eGL_UNSIGNED_INT_IMAGE_1D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_ATOMIC_COUNTER:
            if(IsGLES || IsDstProgramSPIRV)
              // Image uniforms cannot be re-assigned in GLES or with SPIR-V programs.
              break;
          // deliberate fall-through
          // treat all samplers as just an int (since they just store their binding value)
          case eGL_SAMPLER_1D:
          case eGL_SAMPLER_2D:
          case eGL_SAMPLER_3D:
          case eGL_SAMPLER_CUBE:
          case eGL_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_SAMPLER_1D_SHADOW:
          case eGL_SAMPLER_2D_SHADOW:
          case eGL_SAMPLER_1D_ARRAY:
          case eGL_SAMPLER_2D_ARRAY:
          case eGL_SAMPLER_1D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_MULTISAMPLE:
          case eGL_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_SAMPLER_CUBE_SHADOW:
          case eGL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
          case eGL_SAMPLER_BUFFER:
          case eGL_SAMPLER_2D_RECT:
          case eGL_SAMPLER_2D_RECT_SHADOW:
          case eGL_INT_SAMPLER_1D:
          case eGL_INT_SAMPLER_2D:
          case eGL_INT_SAMPLER_3D:
          case eGL_INT_SAMPLER_CUBE:
          case eGL_INT_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_INT_SAMPLER_1D_ARRAY:
          case eGL_INT_SAMPLER_2D_ARRAY:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_SAMPLER_BUFFER:
          case eGL_INT_SAMPLER_2D_RECT:
          case eGL_UNSIGNED_INT_SAMPLER_1D:
          case eGL_UNSIGNED_INT_SAMPLER_2D:
          case eGL_UNSIGNED_INT_SAMPLER_3D:
          case eGL_UNSIGNED_INT_SAMPLER_CUBE:
          case eGL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_BUFFER:
          case eGL_UNSIGNED_INT_SAMPLER_2D_RECT:
          case eGL_INT:
            if(!IsDstProgramSPIRV)    // SPIR-V shaders treat samplers as immutable
              GL.glProgramUniform1iv(progDst, dstLocation, 1, iv);
            break;
          default: RDCERR("Unhandled uniform type '%s'", ToStr(val.Type).c_str());
        }
      }
    }

    if(!IsDstProgramSPIRV)
    {
      // apply UBO bindings
      for(const ProgramBinding &bind : serialisedUniforms.UBOBindings)
      {
        GLuint idx = GL.glGetUniformBlockIndex(progDst, bind.Name.c_str());
        if(idx != GL_INVALID_INDEX)
          GL.glUniformBlockBinding(progDst, idx, bind.Binding);
      }
    }

    // apply SSBO bindings
    // GLES does not allow modification of SSBO bindings - which is good as we don't need to restore
    // them, since they're immutable.
    if(!IsDstProgramSPIRV && !IsGLES)
    {
      for(const ProgramBinding &bind : serialisedUniforms.SSBOBindings)
      {
        GLuint idx =
            GL.glGetProgramResourceIndex(progDst, eGL_SHADER_STORAGE_BLOCK, bind.Name.c_str());
        if(idx != GL_INVALID_INDEX)
        {
          if(GL.glShaderStorageBlockBinding)
          {
            GL.glShaderStorageBlockBinding(progDst, idx, bind.Binding);
          }
          else
          {
            RDCERR("glShaderStorageBlockBinding is not supported!");
          }
        }
      }
    }
  }
}

void CopyProgramUniforms(const PerStageReflections &srcStages, GLuint progSrc,
                         const PerStageReflections &dstStages, GLuint progDst)
{
  const bool CopyUniforms = true;
  const bool SerialiseUniforms = false;
  ForAllProgramUniforms<CopyUniforms, SerialiseUniforms, ReadSerialiser>(
      NULL, CaptureState::ActiveReplaying, srcStages, progSrc, dstStages, progDst, NULL);
}

template <typename SerialiserType>
void SerialiseProgramUniforms(SerialiserType &ser, CaptureState state,
                              const PerStageReflections &stages, GLuint prog,
                              std::map<GLint, GLint> *locTranslate)
{
  const bool CopyUniforms = false;
  const bool SerialiseUniforms = true;
  ForAllProgramUniforms<CopyUniforms, SerialiseUniforms>(&ser, state, stages, prog, stages, prog,
                                                         locTranslate);
}

template void SerialiseProgramUniforms(ReadSerialiser &ser, CaptureState state,
                                       const PerStageReflections &stages, GLuint prog,
                                       std::map<GLint, GLint> *locTranslate);
template void SerialiseProgramUniforms(WriteSerialiser &ser, CaptureState state,
                                       const PerStageReflections &stages, GLuint prog,
                                       std::map<GLint, GLint> *locTranslate);

bool CopyProgramAttribBindings(GLuint progsrc, GLuint progdst, ShaderReflection *refl)
{
  // don't try to copy bindings for SPIR-V shaders. The queries by name may fail, and the bindings
  // are immutable anyway
  if(refl->encoding == ShaderEncoding::SPIRV)
    return false;

  // copy over attrib bindings
  for(const SigParameter &sig : refl->inputSignature)
  {
    // skip built-ins
    if(sig.systemValue != ShaderBuiltin::Undefined)
      continue;

    GLint idx = GL.glGetAttribLocation(progsrc, sig.varName.c_str());
    if(idx >= 0)
      GL.glBindAttribLocation(progdst, (GLuint)idx, sig.varName.c_str());
  }

  return !refl->inputSignature.empty();
}

bool CopyProgramFragDataBindings(GLuint progsrc, GLuint progdst, ShaderReflection *refl)
{
  // don't try to copy bindings for SPIR-V shaders. The queries by name may fail, and the bindings
  // are immutable anyway
  if(refl->encoding == ShaderEncoding::SPIRV)
    return false;

  uint64_t used = 0;

  // copy over fragdata bindings
  for(size_t i = 0; i < refl->outputSignature.size(); i++)
  {
    // only look at colour outputs (should be the only outputs from fs)
    if(refl->outputSignature[i].systemValue != ShaderBuiltin::ColorOutput)
      continue;

    if(!strncmp("gl_", refl->outputSignature[i].varName.c_str(), 3))
      continue;    // GL_INVALID_OPERATION if name starts with reserved gl_ prefix

    GLint idx = GL.glGetFragDataLocation(progsrc, refl->outputSignature[i].varName.c_str());
    if(idx >= 0)
    {
      uint64_t mask = 1ULL << idx;

      if(used & mask)
      {
        RDCWARN("Multiple signatures bound to output %zu, ignoring %s", i,
                refl->outputSignature[i].varName.c_str());
        continue;
      }

      used |= mask;

      if(GL.glBindFragDataLocation)
      {
        GL.glBindFragDataLocation(progdst, (GLuint)idx, refl->outputSignature[i].varName.c_str());
      }
      else
      {
        // glBindFragDataLocation is not core GLES, but it is in GL_EXT_blend_func_extended
        // TODO what to do if that extension is not supported
        RDCWARN("glBindFragDataLocation is not supported!");
      }
    }
  }

  return !refl->outputSignature.empty();
}

template <typename SerialiserType>
bool SerialiseProgramBindings(SerialiserType &ser, CaptureState state,
                              const PerStageReflections &stages, GLuint prog)
{
  std::vector<ProgramBinding> InputBindings;
  std::vector<ProgramBinding> OutputBindings;

  // technically we can completely skip this if the shaders are SPIR-V, but for compatibility we
  // instead just skip the fetch & apply steps, so that we can still serialise in a backwards
  // compatible way.
  bool IsProgramSPIRV = false;
  for(size_t i = 0; i < 6; i++)
    IsProgramSPIRV |= stages.refls[i] && stages.refls[i]->encoding == ShaderEncoding::SPIRV;

  if(ser.IsWriting() && !IsProgramSPIRV)
  {
    char buf[128] = {};

    for(int sigType = 0; sigType < 2; sigType++)
    {
      GLenum sigEnum = (sigType == 0 ? eGL_PROGRAM_INPUT : eGL_PROGRAM_OUTPUT);
      std::vector<ProgramBinding> &bindings = (sigType == 0 ? InputBindings : OutputBindings);

      int32_t NumAttributes = 0;
      GL.glGetProgramInterfaceiv(prog, sigEnum, eGL_ACTIVE_RESOURCES, (GLint *)&NumAttributes);
      bindings.reserve(NumAttributes);

      for(GLint i = 0; i < NumAttributes; i++)
      {
        GL.glGetProgramResourceName(prog, sigEnum, i, 128, NULL, buf);

        ProgramBinding bind;
        bind.Name = buf;

        if(sigType == 0)
          bind.Binding = GL.glGetAttribLocation(prog, buf);
        else
          bind.Binding = GL.glGetFragDataLocation(prog, buf);

        bindings.push_back(bind);
      }
    }
  }

  SERIALISE_ELEMENT(InputBindings);
  SERIALISE_ELEMENT(OutputBindings);

  if(ser.IsReading() && IsReplayMode(state) && !IsProgramSPIRV)
  {
    for(int sigType = 0; sigType < 2; sigType++)
    {
      const std::vector<ProgramBinding> &bindings = (sigType == 0 ? InputBindings : OutputBindings);

      uint64_t used = 0;

      for(const ProgramBinding &bind : bindings)
      {
        if(bind.Binding >= 0)
        {
          uint64_t mask = 1ULL << bind.Binding;

          if(used & mask)
          {
            RDCWARN("Multiple %s items bound to location %d, ignoring %s",
                    sigType == 0 ? "attrib" : "fragdata", bind.Binding, bind.Name.c_str());
            continue;
          }

          used |= mask;

          if(!strncmp("gl_", bind.Name.c_str(), 3))
            continue;    // GL_INVALID_OPERATION if name starts with reserved gl_ prefix (for both
                         // glBindAttribLocation and glBindFragDataLocation)

          if(sigType == 0)
          {
            GL.glBindAttribLocation(prog, (GLuint)bind.Binding, bind.Name.c_str());
          }
          else
          {
            if(GL.glBindFragDataLocation)
            {
              GL.glBindFragDataLocation(prog, (GLuint)bind.Binding, bind.Name.c_str());
            }
            else
            {
              // glBindFragDataLocation is not core GLES, but it is in GL_EXT_blend_func_extended
              // TODO what to do if that extension is not supported
              RDCWARN("glBindFragDataLocation is not supported!");
            }
          }
        }
      }
    }
  }

  return !IsProgramSPIRV && (!InputBindings.empty() || !OutputBindings.empty());
}

template bool SerialiseProgramBindings(ReadSerialiser &ser, CaptureState state,
                                       const PerStageReflections &stages, GLuint prog);
template bool SerialiseProgramBindings(WriteSerialiser &ser, CaptureState state,
                                       const PerStageReflections &stages, GLuint prog);
