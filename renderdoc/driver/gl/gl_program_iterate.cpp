/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
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
    case eGL_INT_VEC4: baseType = VarType::Int; break;
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
      ser.Serialise("data", fv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::Int)
      ser.Serialise("data", iv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::UInt)
      ser.Serialise("data", uv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::Double)
      ser.Serialise("data", dv, elemCount, SerialiserFlags::NoFlags);
  }
  else
  {
    if(baseType == VarType::Double)
      ser.Serialise("data", fv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::Float)
      ser.Serialise("data", dv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::Int)
      ser.Serialise("data", iv, elemCount, SerialiserFlags::NoFlags);
    else if(baseType == VarType::UInt)
      ser.Serialise("data", uv, elemCount, SerialiserFlags::NoFlags);
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

// bit of a hack, to work around C4127: conditional expression is constant
// on template parameters
template <typename T>
T CheckConstParam(T t);
template <>
bool CheckConstParam(bool t)
{
  return t;
}

template <const bool CopyUniforms, const bool SerialiseUniforms, typename SerialiserType>
static void ForAllProgramUniforms(SerialiserType *ser, CaptureState state, const GLHookSet &gl,
                                  GLuint progSrc, GLuint progDst, map<GLint, GLint> *locTranslate)
{
  const bool ReadSourceProgram = CopyUniforms || (SerialiseUniforms && ser && ser->IsWriting());
  const bool WriteDestProgram = CopyUniforms || (SerialiseUniforms && ser && ser->IsReading());

  RDCCOMPILE_ASSERT((CopyUniforms && !SerialiseUniforms) || (!CopyUniforms && SerialiseUniforms),
                    "Invalid call to ForAllProgramUniforms");

  // this struct will be serialised with the uniform binding data, or if we're just copying it will
  // be used to store the data fetched from the source program, before being applied to the
  // destination program. It's slightly redundant since we could unify the loops (as the code used
  // to do) but it's much better for code organisation and clarity to have a single path whether
  // serialising or not.
  ProgramUniforms serialisedUniforms;

  // if we're reading the source program, iterate over the interfaces and fetch the data.
  if(CheckConstParam(ReadSourceProgram))
  {
    const size_t numProps = 5;
    GLenum resProps[numProps] = {
        eGL_BLOCK_INDEX, eGL_TYPE, eGL_NAME_LENGTH, eGL_ARRAY_SIZE, eGL_LOCATION,
    };
    GLint values[numProps];

    GLint NumUniforms = 0;
    gl.glGetProgramInterfaceiv(progSrc, eGL_UNIFORM, eGL_ACTIVE_RESOURCES, &NumUniforms);

    // this is a very conservative figure - many uniforms will be in UBOs and so will be ignored
    serialisedUniforms.ValueUniforms.reserve(NumUniforms);

    for(GLint i = 0; i < NumUniforms; i++)
    {
      GLenum type = eGL_NONE;
      int32_t arraySize = 0;
      int32_t srcLocation = 0;
      string basename;
      bool isArray = false;

      gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM, i, numProps, resProps, numProps, NULL, values);

      // we don't need to consider uniforms within UBOs
      if(values[0] >= 0)
        continue;

      // get the metadata we need for fetching the data
      type = (GLenum)values[1];
      arraySize = values[3];
      srcLocation = values[4];

      char n[1024] = {0};
      gl.glGetProgramResourceName(progSrc, eGL_UNIFORM, i, values[2], NULL, n);

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

      // loop over every element in the array (arraySize = 1 for non arrays)
      for(GLint arr = 0; arr < arraySize; arr++)
      {
        ProgramUniformValue &uniformVal = uniform.Values[arr];
        uniformVal.Type = type;
        uniformVal.Location = srcLocation;

        std::string name = basename;

        // append the subscript if this item is an array.
        if(isArray)
        {
          name += StringFormat::Fmt("[%d]", arr);

          uniformVal.Location = srcLocation = gl.glGetUniformLocation(progSrc, name.c_str());
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
          case eGL_FLOAT_VEC4: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
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
            gl.glGetUniformdv(progSrc, srcLocation, dv);
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
          case eGL_INT_VEC4:
            gl.glGetUniformiv(progSrc, srcLocation, iv);
            break;
          // bools are unsigned integers
          case eGL_UNSIGNED_INT:
          case eGL_BOOL:
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_BOOL_VEC2:
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_BOOL_VEC3:
          case eGL_UNSIGNED_INT_VEC4:
          case eGL_BOOL_VEC4: gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
          default: RDCERR("Unhandled uniform type '%s'", ToStr(type).c_str());
        }
      }
    }

    // now find how many UBOs we have, and store their binding indices
    GLint numUBOs = 0;
    gl.glGetProgramInterfaceiv(progSrc, eGL_UNIFORM_BLOCK, eGL_ACTIVE_RESOURCES, &numUBOs);

    serialisedUniforms.UBOBindings.reserve(numUBOs);

    for(GLint i = 0; i < numUBOs; i++)
    {
      GLenum prop = eGL_BUFFER_BINDING;
      uint32_t bind = 0;

      gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM_BLOCK, i, 1, &prop, 1, NULL, (GLint *)&bind);

      char n[1024] = {0};
      gl.glGetProgramResourceName(progSrc, eGL_UNIFORM_BLOCK, i, 1023, NULL, n);

      serialisedUniforms.UBOBindings.push_back(ProgramBinding(n, bind));
    }

    // finally, if SSBOs are supported on this implementation, fetch their bindings
    GLint numSSBOs = 0;
    if(HasExt[ARB_shader_storage_buffer_object])
      gl.glGetProgramInterfaceiv(progSrc, eGL_SHADER_STORAGE_BLOCK, eGL_ACTIVE_RESOURCES, &numSSBOs);

    serialisedUniforms.SSBOBindings.reserve(numSSBOs);

    for(GLint i = 0; i < numSSBOs; i++)
    {
      GLenum prop = eGL_BUFFER_BINDING;
      uint32_t bind = 0;

      gl.glGetProgramResourceiv(progSrc, eGL_SHADER_STORAGE_BLOCK, i, 1, &prop, 1, NULL,
                                (GLint *)&bind);

      char n[1024] = {0};
      gl.glGetProgramResourceName(progSrc, eGL_SHADER_STORAGE_BLOCK, i, 1023, NULL, n);

      serialisedUniforms.SSBOBindings.push_back(ProgramBinding(n, bind));
    }
  }

  // now serialise all the bindings if we are serialising
  if(CheckConstParam(SerialiseUniforms) && ser)
  {
    ser->Serialise("ProgramUniforms", serialisedUniforms);
  }

  // if we are writing to a destination program and replaying, then apply the stored data from
  // serialisedUniforms
  if(CheckConstParam(WriteDestProgram) && IsReplayMode(state))
  {
    // loop over the loose global uniforms, see if there is an equivalent, and apply it.
    for(const ProgramUniform &uniform : serialisedUniforms.ValueUniforms)
    {
      for(size_t arr = 0; arr < uniform.Values.size(); arr++)
      {
        const ProgramUniformValue &val = uniform.Values[arr];

        std::string name = uniform.Basename;

        if(uniform.IsArray)
          name += StringFormat::Fmt("[%u]", (uint32_t)arr);

        GLint dstLocation = gl.glGetUniformLocation(progDst, name.c_str());
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
            gl.glProgramUniformMatrix4fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT4x3:
            gl.glProgramUniformMatrix4x3fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT4x2:
            gl.glProgramUniformMatrix4x2fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT3:
            gl.glProgramUniformMatrix3fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT3x4:
            gl.glProgramUniformMatrix3x4fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT3x2:
            gl.glProgramUniformMatrix3x2fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT2:
            gl.glProgramUniformMatrix2fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT2x4:
            gl.glProgramUniformMatrix2x4fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_FLOAT_MAT2x3:
            gl.glProgramUniformMatrix2x3fv(progDst, dstLocation, 1, false, fv);
            break;
          case eGL_DOUBLE_MAT4:
            gl.glProgramUniformMatrix4dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT4x3:
            gl.glProgramUniformMatrix4x3dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT4x2:
            gl.glProgramUniformMatrix4x2dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT3:
            gl.glProgramUniformMatrix3dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT3x4:
            gl.glProgramUniformMatrix3x4dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT3x2:
            gl.glProgramUniformMatrix3x2dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT2:
            gl.glProgramUniformMatrix2dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT2x4:
            gl.glProgramUniformMatrix2x4dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT2x3:
            gl.glProgramUniformMatrix2x3dv(progDst, dstLocation, 1, false, dv);
            break;
          case eGL_FLOAT: gl.glProgramUniform1fv(progDst, dstLocation, 1, fv); break;
          case eGL_FLOAT_VEC2: gl.glProgramUniform2fv(progDst, dstLocation, 1, fv); break;
          case eGL_FLOAT_VEC3: gl.glProgramUniform3fv(progDst, dstLocation, 1, fv); break;
          case eGL_FLOAT_VEC4: gl.glProgramUniform4fv(progDst, dstLocation, 1, fv); break;
          case eGL_DOUBLE: gl.glProgramUniform1dv(progDst, dstLocation, 1, dv); break;
          case eGL_DOUBLE_VEC2: gl.glProgramUniform2dv(progDst, dstLocation, 1, dv); break;
          case eGL_DOUBLE_VEC3: gl.glProgramUniform3dv(progDst, dstLocation, 1, dv); break;
          case eGL_DOUBLE_VEC4: gl.glProgramUniform4dv(progDst, dstLocation, 1, dv); break;

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
            if(IsGLES)
              // Image uniforms cannot be re-assigned in GLES.
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
          case eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_BUFFER:
          case eGL_UNSIGNED_INT_SAMPLER_2D_RECT:
          case eGL_INT: gl.glProgramUniform1iv(progDst, dstLocation, 1, iv); break;
          case eGL_INT_VEC2: gl.glProgramUniform2iv(progDst, dstLocation, 1, iv); break;
          case eGL_INT_VEC3: gl.glProgramUniform3iv(progDst, dstLocation, 1, iv); break;
          case eGL_INT_VEC4: gl.glProgramUniform4iv(progDst, dstLocation, 1, iv); break;
          case eGL_UNSIGNED_INT:
          case eGL_BOOL: gl.glProgramUniform1uiv(progDst, dstLocation, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_BOOL_VEC2: gl.glProgramUniform2uiv(progDst, dstLocation, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_BOOL_VEC3: gl.glProgramUniform3uiv(progDst, dstLocation, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC4:
          case eGL_BOOL_VEC4: gl.glProgramUniform4uiv(progDst, dstLocation, 1, uiv); break;
          default: RDCERR("Unhandled uniform type '%s'", ToStr(val.Type).c_str());
        }
      }
    }

    // apply UBO bindings
    for(const ProgramBinding &bind : serialisedUniforms.UBOBindings)
    {
      GLuint idx = gl.glGetUniformBlockIndex(progDst, bind.Name.c_str());
      if(idx != GL_INVALID_INDEX)
        gl.glUniformBlockBinding(progDst, idx, bind.Binding);
    }

    // apply SSBO bindings
    for(const ProgramBinding &bind : serialisedUniforms.SSBOBindings)
    {
      GLuint idx = gl.glGetProgramResourceIndex(progDst, eGL_SHADER_STORAGE_BLOCK, bind.Name.c_str());
      if(idx != GL_INVALID_INDEX)
      {
        if(gl.glShaderStorageBlockBinding)
        {
          gl.glShaderStorageBlockBinding(progDst, idx, bind.Binding);
        }
        else
        {
          // TODO glShaderStorageBlockBinding is not core GLES
          RDCERR("glShaderStorageBlockBinding is not supported!");
        }
      }
    }
  }
}

void CopyProgramUniforms(const GLHookSet &gl, GLuint progSrc, GLuint progDst)
{
  const bool CopyUniforms = true;
  const bool SerialiseUniforms = false;
  ForAllProgramUniforms<CopyUniforms, SerialiseUniforms, ReadSerialiser>(
      NULL, CaptureState::ActiveReplaying, gl, progSrc, progDst, NULL);
}

template <typename SerialiserType>
void SerialiseProgramUniforms(SerialiserType &ser, CaptureState state, const GLHookSet &gl,
                              GLuint prog, map<GLint, GLint> *locTranslate)
{
  const bool CopyUniforms = false;
  const bool SerialiseUniforms = true;
  ForAllProgramUniforms<CopyUniforms, SerialiseUniforms>(&ser, state, gl, prog, prog, locTranslate);
}

template void SerialiseProgramUniforms(ReadSerialiser &ser, CaptureState state, const GLHookSet &gl,
                                       GLuint prog, map<GLint, GLint> *locTranslate);
template void SerialiseProgramUniforms(WriteSerialiser &ser, CaptureState state, const GLHookSet &gl,
                                       GLuint prog, map<GLint, GLint> *locTranslate);

void CopyProgramAttribBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst,
                               ShaderReflection *refl)
{
  // copy over attrib bindings
  for(const SigParameter &sig : refl->inputSignature)
  {
    // skip built-ins
    if(sig.systemValue != ShaderBuiltin::Undefined)
      continue;

    GLint idx = gl.glGetAttribLocation(progsrc, sig.varName.c_str());
    if(idx >= 0)
      gl.glBindAttribLocation(progdst, (GLuint)idx, sig.varName.c_str());
  }
}

void CopyProgramFragDataBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst,
                                 ShaderReflection *refl)
{
  uint64_t used = 0;

  // copy over fragdata bindings
  for(size_t i = 0; i < refl->outputSignature.size(); i++)
  {
    // only look at colour outputs (should be the only outputs from fs)
    if(refl->outputSignature[i].systemValue != ShaderBuiltin::ColorOutput)
      continue;

    if(!strncmp("gl_", refl->outputSignature[i].varName.c_str(), 3))
      continue;    // GL_INVALID_OPERATION if name starts with reserved gl_ prefix

    GLint idx = gl.glGetFragDataLocation(progsrc, refl->outputSignature[i].varName.c_str());
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

      if(gl.glBindFragDataLocation)
      {
        gl.glBindFragDataLocation(progdst, (GLuint)idx, refl->outputSignature[i].varName.c_str());
      }
      else
      {
        // glBindFragDataLocation is not core GLES, but it is in GL_EXT_blend_func_extended
        // TODO what to do if that extension is not supported
        RDCERR("glBindFragDataLocation is not supported!");
      }
    }
  }
}

template <typename SerialiserType>
void SerialiseProgramBindings(SerialiserType &ser, CaptureState state, const GLHookSet &gl,
                              GLuint prog)
{
  std::vector<ProgramBinding> InputBindings;
  std::vector<ProgramBinding> OutputBindings;

  if(ser.IsWriting())
  {
    char buf[128] = {};

    for(int sigType = 0; sigType < 2; sigType++)
    {
      GLenum sigEnum = (sigType == 0 ? eGL_PROGRAM_INPUT : eGL_PROGRAM_OUTPUT);
      std::vector<ProgramBinding> &bindings = (sigType == 0 ? InputBindings : OutputBindings);

      int32_t NumAttributes = 0;
      gl.glGetProgramInterfaceiv(prog, sigEnum, eGL_ACTIVE_RESOURCES, (GLint *)&NumAttributes);
      bindings.reserve(NumAttributes);

      for(GLint i = 0; i < NumAttributes; i++)
      {
        gl.glGetProgramResourceName(prog, sigEnum, i, 128, NULL, buf);

        ProgramBinding bind;
        bind.Name = buf;

        if(sigType == 0)
          bind.Binding = gl.glGetAttribLocation(prog, buf);
        else
          bind.Binding = gl.glGetFragDataLocation(prog, buf);

        bindings.push_back(bind);
      }
    }
  }

  SERIALISE_ELEMENT(InputBindings);
  SERIALISE_ELEMENT(OutputBindings);

  if(ser.IsReading() && IsReplayMode(state))
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
            gl.glBindAttribLocation(prog, (GLuint)bind.Binding, bind.Name.c_str());
          }
          else
          {
            if(gl.glBindFragDataLocation)
            {
              gl.glBindFragDataLocation(prog, (GLuint)bind.Binding, bind.Name.c_str());
            }
            else
            {
              // glBindFragDataLocation is not core GLES, but it is in GL_EXT_blend_func_extended
              // TODO what to do if that extension is not supported
              RDCERR("glBindFragDataLocation is not supported!");
            }
          }
        }
      }
    }
  }
}

template void SerialiseProgramBindings(ReadSerialiser &ser, CaptureState state, const GLHookSet &gl,
                                       GLuint prog);
template void SerialiseProgramBindings(WriteSerialiser &ser, CaptureState state,
                                       const GLHookSet &gl, GLuint prog);
