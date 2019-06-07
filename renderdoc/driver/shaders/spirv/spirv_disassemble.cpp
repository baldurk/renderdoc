/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include <limits.h>
#include <algorithm>
#include <utility>
#include "api/replay/renderdoc_replay.h"
#include "common/common.h"
#include "maths/formatpacking.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "spirv_common.h"

#undef min
#undef max

#include "3rdparty/glslang/SPIRV/GLSL.std.450.h"
#include "3rdparty/glslang/SPIRV/spirv.hpp"
#include "3rdparty/glslang/glslang/Public/ShaderLang.h"

#include "spirv_common.h"
#include "spirv_reflect.h"

// need to manually put these headers in the spv namespace
namespace spv
{
#include "3rdparty/glslang/SPIRV/GLSL.ext.AMD.h"
#include "3rdparty/glslang/SPIRV/GLSL.ext.KHR.h"
#include "3rdparty/glslang/SPIRV/GLSL.ext.NV.h"
};

// I'm not sure yet if this makes things clearer or worse. On the one hand
// it is explicit about stores/loads through pointers, but on the other it
// produces a lot of noise.
#define LOAD_STORE_CONSTRUCTORS 0

// possibly for consistency all constants should construct themselves, but
// for scalars it's potentially simpler just to drop it.
#define SCALAR_CONSTRUCTORS 0

// output extension instruction calls as 'GLSL.std.450::FAbs()' instead of
// just 'abs()'
#define USE_CANONICAL_EXT_INST_NAMES 0

// don't inline expressions of this complexity or higher
#define NO_INLINE_COMPLEXITY 5

// used for indicating that an operation must never be inlined
#define NEVER_INLINE_COMPLEXITY 1000

// declare function variables at the top of the scope, rather than at the
// first use of that variable
#define C_VARIABLE_DECLARATIONS 0

// show the offset/arraystride/matrixstride decorations for structure packing
#define SHOW_STRUCT_PACKING 0

#define INVALID_BIND -INT_MAX

namespace spv
{
Op OpUnknown = (Op)~0U;
}

const char *GLSL_STD_450_names[] = {
    "-",    // Bad

    "Round",
    "RoundEven",
    "Trunc",
    "FAbs",
    "SAbs",
    "FSign",
    "SSign",
    "Floor",
    "Ceil",
    "Fract",

    "Radians",
    "Degrees",
    "Sin",
    "Cos",
    "Tan",
    "Asin",
    "Acos",
    "Atan",
    "Sinh",
    "Cosh",
    "Tanh",
    "Asinh",
    "Acosh",
    "Atanh",
    "Atan2",

    "Pow",
    "Exp",
    "Log",
    "Exp2",
    "Log2",
    "Sqrt",
    "InverseSqrt",

    "Determinant",
    "MatrixInverse",

    "Modf",
    "ModfStruct",
    "FMin",
    "UMin",
    "SMin",
    "FMax",
    "UMax",
    "SMax",
    "FClamp",
    "UClamp",
    "SClamp",
    "FMix",
    "IMix",
    "Step",
    "SmoothStep",

    "Fma",
    "Frexp",
    "FrexpStruct",
    "Ldexp",

    "PackSnorm4x8",
    "PackUnorm4x8",
    "PackSnorm2x16",
    "PackUnorm2x16",
    "PackHalf2x16",
    "PackDouble2x32",
    "UnpackSnorm2x16",
    "UnpackUnorm2x16",
    "UnpackHalf2x16",
    "UnpackSnorm4x8",
    "UnpackUnorm4x8",
    "UnpackDouble2x32",

    "Length",
    "Distance",
    "Cross",
    "Normalize",
    "FaceForward",
    "Reflect",
    "Refract",

    "FindILsb",
    "FindSMsb",
    "FindUMsb",

    "InterpolateAtCentroid",
    "InterpolateAtSample",
    "InterpolateAtOffset",

    "NMin",
    "NMax",
    "NClamp",
};

const char *GLSL_STD_450_friendly_names[] = {
    "-",    // Bad

    "round",
    "roundEven",
    "trunc",
    "abs",
    "abs",
    "sign",
    "sign",
    "floor",
    "ceil",
    "fract",

    "radians",
    "degrees",
    "sin",
    "cos",
    "tan",
    "asin",
    "acos",
    "atan",
    "sinh",
    "cosh",
    "tanh",
    "asinh",
    "acosh",
    "atanh",
    "atan2",

    "pow",
    "exp",
    "log",
    "exp2",
    "log2",
    "sqrt",
    "inversesqrt",

    "determinant",
    "inverse",

    "modf",
    "modf",
    "min",
    "min",
    "min",
    "max",
    "max",
    "max",
    "clamp",
    "clamp",
    "clamp",
    "mix",
    "mix",
    "step",
    "smoothstep",

    "fma",
    "frexp",
    "frexp",
    "ldexp",

    "packSnorm4x8",
    "packUnorm4x8",
    "packSnorm2x16",
    "packUnorm2x16",
    "packHalf2x16",
    "packDouble2x32",
    "unpackSnorm2x16",
    "unpackUnorm2x16",
    "unpackHalf2x16",
    "unpackSnorm4x8",
    "unpackUnorm4x8",
    "unpackDouble2x32",

    "length",
    "distance",
    "cross",
    "normalize",
    "faceforward",
    "reflect",
    "refract",

    "findLSB",
    "findMSB",
    "findMSB",

    "interpolateAtCentroid",
    "interpolateAtSample",
    "interpolateAtOffset",

    "min",
    "max",
    "clamp",
};

RDCCOMPILE_ASSERT(ARRAY_COUNT(GLSL_STD_450_names) == GLSLstd450Count,
                  "Wrong number of GLSL extension function names");
RDCCOMPILE_ASSERT(ARRAY_COUNT(GLSL_STD_450_friendly_names) == GLSLstd450Count,
                  "Wrong number of GLSL extension function names");

// https://www.khronos.org/registry/spir-v/api/spir-v.xml
struct GeneratorID
{
  uint32_t toolid;
  const char *vendor;
  const char *tool;
  const char *comment;
} KnownGenerators[] = {
    // 0 reserved
    {1, "LunarG", "Unknown", "Contact TBD"},
    {2, "Valve", "Unknown", "Contact TBD"},
    {3, "Codeplay", "Unknown", "Contact Neil Henning, neil@codeplay.com"},
    {4, "NVIDIA", "Unknown", "Contact Kerch Holt, kholt@nvidia.com"},
    {5, "ARM", "Unknown", "Contact Alexander Galazin, alexander.galazin@arm.com"},
    {6, "Khronos", "LLVM/SPIR-V Translator", "Contact Yaxun (Sam) Liu, yaxun.liu@amd.com"},
    {7, "Khronos", "SPIR-V Tools Assembler", "Contact David Neto, dneto@google.com"},
    {8, "Khronos", "Glslang Reference Front End",
     "Contact John Kessenich, johnkessenich@google.com"},
    {9, "Qualcomm", "Unknown", "Contact weifengz@qti.qualcomm.com"},
    {10, "AMD", "Unknown", "Contact Daniel Rakos, daniel.rakos@amd.com"},
    {11, "Intel", "Unknown", "Contact Alexey, alexey.bader@intel.com"},
    {12, "Imagination", "Unknown", "Contact James Jones"},
    {13, "Google", "Shaderc over Glslang", "Contact David Neto, dneto@google.com"},
    {14, "Google", "spiregg", "Contact Lei Zhang, antiagainst@google.com"},
    {15, "Google", "rspirv", "Contact Lei Zhang, antiagainst@gmail.com"},
    {16, "X-LEGEND", "Mesa-IR/SPIR-V Translator",
     "Contact Metora Wang, github:metora/MesaGLSLCompiler"},
    {17, "Khronos", "SPIR-V Tools Linker", "Contact David Neto, dneto@google.com"},
    {18, "Wine", "VKD3D Shader Compiler", "Contact wine-devel@winehq.org"},
    {19, "Clay", "Clay Shader Compiler", "Contact info@clayengine.com"},
    {20, "W3C WebGPU Group", "WHLSL Shader Translator", "https://github.com/gpuweb/WHLSL"},
};

template <typename EnumType>
static std::string OptionalFlagString(EnumType e)
{
  return (int)e ? " [" + ToStr(e) + "]" : "";
}

static std::string DefaultIDName(uint32_t ID)
{
  return StringFormat::Fmt("_%u_", ID);
}

template <typename T>
static bool erase_item(std::vector<T> &vec, const T &elem)
{
  auto it = std::find(vec.begin(), vec.end(), elem);
  if(it != vec.end())
  {
    vec.erase(it);
    return true;
  }

  return false;
}

struct SPVInstruction;
struct SPVConstant;

struct SPVDecoration
{
  SPVDecoration() : decoration(spv::DecorationRelaxedPrecision), val(0) {}
  spv::Decoration decoration;

  uint32_t val;

  std::string Str() const
  {
    switch(decoration)
    {
      case spv::DecorationRowMajor:
      case spv::DecorationColMajor:
      case spv::DecorationNoPerspective:
      case spv::DecorationFlat:
      case spv::DecorationPatch:
      case spv::DecorationCentroid:
      case spv::DecorationSample:
      case spv::DecorationGLSLShared:
      case spv::DecorationBlock:
      case spv::DecorationBufferBlock:
      case spv::DecorationRelaxedPrecision:
      case spv::DecorationInvariant:
      case spv::DecorationRestrict:
      case spv::DecorationVolatile:
      case spv::DecorationAliased:
      case spv::DecorationCoherent:
      case spv::DecorationNonWritable:
      case spv::DecorationNonReadable: return ToStr(decoration);
      case spv::DecorationUniform: return StringFormat::Fmt("DynamicallyUniform", val);
      case spv::DecorationLocation: return StringFormat::Fmt("Location=%u", val);
      case spv::DecorationComponent: return StringFormat::Fmt("Location=%u", val);
      case spv::DecorationBinding: return StringFormat::Fmt("Bind=%u", val);
      case spv::DecorationIndex: return StringFormat::Fmt("Index=%u", val);
      case spv::DecorationStream: return StringFormat::Fmt("Stream=%u", val);
      case spv::DecorationDescriptorSet: return StringFormat::Fmt("DescSet=%u", val);
      case spv::DecorationBuiltIn:
        return StringFormat::Fmt("Builtin %s", ToStr((spv::BuiltIn)val).c_str());
      case spv::DecorationSpecId:
        return "";    // handled elsewhere

#if SHOW_STRUCT_PACKING
      case spv::DecorationOffset: return StringFormat::Fmt("Offset=%u", val);
      case spv::DecorationArrayStride: return StringFormat::Fmt("ArrayStride=%u", val);
      case spv::DecorationMatrixStride: return StringFormat::Fmt("MatrixStride=%u", val);
#else
      case spv::DecorationOffset:
      case spv::DecorationArrayStride:
      case spv::DecorationMatrixStride: return "";
#endif

      default: break;
    }

    return StringFormat::Fmt("%s=%u", ToStr(decoration).c_str(), val);
  }
};

struct SPVExtInstSet
{
  SPVExtInstSet() : canonicalNames(NULL), friendlyNames(NULL) {}
  std::string setname;
  const char **canonicalNames;
  const char **friendlyNames;
};

struct SPVExecutionMode
{
  SPVExecutionMode() : mode(spv::ExecutionModeInvocations), x(0), y(0), z(0) {}
  spv::ExecutionMode mode;
  uint32_t x, y, z;    // optional params
};

struct SPVEntryPoint
{
  SPVEntryPoint() : func(0), model(spv::ExecutionModelVertex) {}
  // entry point will come before declaring instruction,
  // so we reference the function by ID
  uint32_t func;
  spv::ExecutionModel model;
  std::string name;
  std::vector<SPVExecutionMode> modes;
};

struct SPVTypeData
{
  SPVTypeData()
      : type(eVoid),
        baseType(NULL),
        storage(spv::StorageClassUniformConstant),
        decorations(NULL),
        texdim(spv::Dim2D),
        sampled(2),
        arrayed(false),
        depth(false),
        multisampled(false),
        imgformat(spv::ImageFormatUnknown),
        bitCount(32),
        vectorSize(1),
        matrixSize(1),
        arraySize(1)
  {
  }

  enum
  {
    eVoid,
    eBool,
    eFloat,
    eSInt,
    eUInt,
    eBasicCount,

    eVector,
    eMatrix,
    eArray,
    ePointer,
    eCompositeCount,

    eFunction,

    eStruct,
    eImage,
    eSampler,
    eSampledImage,

    eTypeCount,
  } type;

  SPVTypeData *baseType;

  uint32_t id;

  std::string name;

  bool IsBasicInt() const { return type == eUInt || type == eSInt; }
  bool IsScalar() const { return type < eBasicCount && type != eVoid; }
  std::string DeclareVariable(const std::vector<SPVDecoration> &vardecorations,
                              const std::string &varName);

  const std::string &GetName()
  {
    if(name.empty())
    {
      if(type == eVoid)
      {
        name = "void";
      }
      else if(type == eBool)
      {
        name = "bool";
      }
      else if(type == eFloat)
      {
        RDCASSERT(bitCount == 64 || bitCount == 32 || bitCount == 16);
        name = bitCount == 64 ? "double" : bitCount == 32 ? "float" : "half";
      }
      else if(type == eSInt)
      {
        RDCASSERT(bitCount == 64 || bitCount == 32 || bitCount == 16 || bitCount == 8);
        name = bitCount == 64 ? "long" : bitCount == 32 ? "int" : bitCount == 16 ? "short" : "byte";
      }
      else if(type == eUInt)
      {
        RDCASSERT(bitCount == 64 || bitCount == 32 || bitCount == 16 || bitCount == 8);
        name = bitCount == 64 ? "ulong" : bitCount == 32 ? "uint" : bitCount == 16 ? "ushort"
                                                                                   : "ubyte";
      }
      else if(type == eVector)
      {
        name = StringFormat::Fmt("%s%u", baseType->GetName().c_str(), vectorSize);
      }
      else if(type == eMatrix)
      {
        name = StringFormat::Fmt("%s%ux%u", baseType->GetName().c_str(), matrixSize, vectorSize);
      }
      else if(type == ePointer)
      {
        name = StringFormat::Fmt("%s*", baseType->GetName().c_str());
      }
      else if(type == eArray)
      {
        name = StringFormat::Fmt("%s[%u]", baseType->GetName().c_str(), arraySize);
      }
      else if(type == eImage)
      {
        std::string typestring = baseType->GetName();
        if(imgformat != spv::ImageFormatUnknown)
          typestring += ", " + ToStr(imgformat);

        name = StringFormat::Fmt("%sImage%s%s%s<%s>", depth ? "Depth" : "", multisampled ? "MS" : "",
                                 arrayed ? "Array" : "", ToStr(texdim).c_str(), typestring.c_str());
      }
      else if(type == eSampledImage)
      {
        name = "Sampled" + baseType->GetName();
      }
      else if(type == eSampler)
      {
        name = "Sampler";
      }
      else if(type == eStruct)
      {
        name = StringFormat::Fmt("Anonymous_Struct_%p", this);
      }
      else
      {
        RDCERR("Unexpected type!");
        name = StringFormat::Fmt("Unhandled_%u_Type", type);
      }
    }

    return name;
  }

  VarType GetVarType()
  {
    if(type == SPVTypeData::eFloat)
    {
      if(bitCount == 64)
        return VarType::Double;
      else if(bitCount == 32)
        return VarType::Float;
      else if(bitCount == 16)
        return VarType::Half;

      RDCERR("Unexpected float bitcount: %u", bitCount);

      return VarType::Float;
    }
    else if(type == SPVTypeData::eBool)
    {
      // we treat bools as VkBool32 externally
      return VarType::UInt;
    }
    else if(type == SPVTypeData::eUInt)
    {
      if(bitCount == 64)
        return VarType::ULong;
      else if(bitCount == 32)
        return VarType::UInt;
      else if(bitCount == 16)
        return VarType::UShort;
      else if(bitCount == 8)
        return VarType::UByte;

      RDCERR("Unexpected uint bitcount: %u", bitCount);

      return VarType::UInt;
    }
    else if(type == SPVTypeData::eSInt)
    {
      if(bitCount == 64)
        return VarType::SLong;
      else if(bitCount == 32)
        return VarType::SInt;
      else if(bitCount == 16)
        return VarType::SShort;
      else if(bitCount == 8)
        return VarType::SByte;

      RDCERR("Unexpected sint bitcount: %u", bitCount);

      return VarType::SInt;
    }

    RDCERR("Unexpected base type variable %u", type);

    return VarType::Unknown;
  }

  std::vector<SPVDecoration> *decorations;

  // struct/function
  std::vector<rdcpair<SPVTypeData *, std::string> > children;
  std::vector<std::vector<SPVDecoration> > childDecorations;    // matches children

  // array
  SPVConstant *arraySizeConst = NULL;

  // pointer
  spv::StorageClass storage;

  // sampler/texture/whatever
  spv::Dim texdim;
  uint32_t sampled;
  bool arrayed;
  bool depth;
  bool multisampled;
  spv::ImageFormat imgformat;

  // ints and floats
  uint32_t bitCount;

  uint32_t vectorSize;
  uint32_t matrixSize;
  uint32_t arraySize;
};

struct SPVOperation
{
  SPVOperation()
      : type(NULL),
        access(spv::MemoryAccessMaskNone),
        scope(spv::ScopeDevice),
        scopeMemory(spv::ScopeDevice),
        semantics(spv::MemorySemanticsMaskNone),
        semanticsUnequal(spv::MemorySemanticsMaskNone),
        funcCall(0),
        complexity(0),
        mathop(false),
        inlineArgs(0)
  {
    RDCEraseEl(im);
  }

  SPVTypeData *type;

  // OpLoad/OpStore/OpCopyMemory
  spv::MemoryAccessMask access;

  // OpAtomic*, Op*Barrier
  spv::Scope scope, scopeMemory;
  spv::MemorySemanticsMask semantics, semanticsUnequal;

  // OpExtInst
  std::vector<uint32_t> literals;

  // OpFunctionCall
  uint32_t funcCall;

  // this is modified on the fly, it's used as a measure of whether we
  // can combine multiple statements into one line when displaying the
  // disassembly.
  int complexity;

  // if this operation will be of the form 'a + b', we need to be sure
  // to bracket any arguments that are mathops in nested expressions,
  // to make order of operations clear.
  bool mathop;

  // bitfield indicating which arguments should be inlined
  uint32_t inlineArgs;

  // arguments always reference IDs that already exist (branch/flow
  // control type statements aren't SPVOperations)
  std::vector<SPVInstruction *> arguments;

  struct
  {
    SPVInstruction *bias;
    SPVInstruction *lod;
    SPVInstruction *dx, *dy;
    SPVInstruction *constOffset;
    SPVInstruction *offset;
    SPVInstruction *gatherOffsets;
    SPVInstruction *sampleIdx;
    SPVInstruction *minLod;
  } im;

  void GetArg(const std::vector<SPVInstruction *> &ids, size_t idx, std::string &arg,
              bool bracketArgumentsIfNeeded = true);
};

struct SPVConstant
{
  SPVConstant() : type(NULL), specialized(false), specOp(spv::OpNop), u64(0) {}
  struct SamplerData
  {
    spv::SamplerAddressingMode addressing;
    bool normalised;
    spv::SamplerFilterMode filter;
  };

  SPVTypeData *type;
  bool specialized;
  spv::Op specOp;
  union
  {
    uint64_t u64;
    uint32_t u32;
    uint16_t u16;
    uint8_t u8;
    int64_t i64;
    int32_t i32;
    int16_t i16;
    int8_t i8;
    float f;
    double d;
    SamplerData sampler;
  };

  std::vector<SPVConstant *> children;

  uint32_t EvaluateIntValue()
  {
    if(specOp != spv::OpNop)
    {
      switch(specOp)
      {
        case spv::OpIAdd:
          if(children.size() == 2)
            return children[0]->EvaluateIntValue() + children[1]->EvaluateIntValue();
          break;
        case spv::OpISub:
          if(children.size() == 2)
            return children[0]->EvaluateIntValue() - children[1]->EvaluateIntValue();
          break;
        case spv::OpIMul:
          if(children.size() == 2)
            return children[0]->EvaluateIntValue() * children[1]->EvaluateIntValue();
          break;
        case spv::OpUDiv:
        case spv::OpSDiv:
          if(children.size() == 2)
            return children[0]->EvaluateIntValue() / children[1]->EvaluateIntValue();
          break;
        default: break;
      }

      RDCERR("Unhandled spec constant op %s", ToStr(specOp).c_str());
    }

    return u32;
  }

  std::string GetValString()
  {
    RDCASSERT(children.empty());

    if(type->type == SPVTypeData::eFloat)
    {
      // @ is a custom printf flag that ensures we always print .0
      // after a float, but without restricting precision or sigfigs
      if(type->bitCount == 64)
        return StringFormat::Fmt("%@lgf", d);
      if(type->bitCount == 32)
        return StringFormat::Fmt("%@gf", f);
      if(type->bitCount == 16)
        return StringFormat::Fmt("%@gf", ConvertFromHalf(u16));
    }
    else if(type->type == SPVTypeData::eSInt)
    {
      if(type->bitCount == 64)
        return StringFormat::Fmt("%lli", i64);
      if(type->bitCount == 32)
        return StringFormat::Fmt("%i", i32);
      if(type->bitCount == 16)
        return StringFormat::Fmt("%hi", i16);
      if(type->bitCount == 8)
        return StringFormat::Fmt("%hhi", i8);
    }
    else if(type->type == SPVTypeData::eUInt)
    {
      if(type->bitCount == 64)
        return StringFormat::Fmt("%llu", u64);
      if(type->bitCount == 32)
        return StringFormat::Fmt("%u", u32);
      if(type->bitCount == 16)
        return StringFormat::Fmt("%hu", u16);
      if(type->bitCount == 8)
        return StringFormat::Fmt("%hhu", u8);
    }
    else if(type->type == SPVTypeData::eBool)
      return u32 ? "true" : "false";

    return StringFormat::Fmt("!%u!", u32);
  }

  std::string GetIDName()
  {
    if((specOp == spv::OpIAdd || specOp == spv::OpFAdd) && children.size() == 2)
    {
      return StringFormat::Fmt("(%s + %s)", children[0]->GetIDName().c_str(),
                               children[1]->GetIDName().c_str());
    }
    else if((specOp == spv::OpISub || specOp == spv::OpFSub) && children.size() == 2)
    {
      return StringFormat::Fmt("(%s - %s)", children[0]->GetIDName().c_str(),
                               children[1]->GetIDName().c_str());
    }
    else if((specOp == spv::OpIMul || specOp == spv::OpFMul) && children.size() == 2)
    {
      return StringFormat::Fmt("(%s * %s)", children[0]->GetIDName().c_str(),
                               children[1]->GetIDName().c_str());
    }
    else if((specOp == spv::OpUDiv || specOp == spv::OpSDiv || specOp == spv::OpFDiv) &&
            children.size() == 2)
    {
      return StringFormat::Fmt("(%s / %s)", children[0]->GetIDName().c_str(),
                               children[1]->GetIDName().c_str());
    }
    else if(specOp != spv::OpNop)
    {
      std::string ret = StringFormat::Fmt("SpecOp%s(", ToStr(specOp).c_str());

      for(size_t i = 0; i < children.size(); i++)
      {
        if(i != 0)
          ret += ", ";

        ret += children[i]->GetIDName();
      }

      ret += ")";
      return ret;
    }

    if(type->IsScalar())
    {
      return GetValString();
    }

    // special case vectors with the same constant
    // replicated across all channels
    if(type->type == SPVTypeData::eVector)
    {
      bool identical = true;
      for(size_t i = 1; i < children.size(); i++)
      {
        if(children[i] == NULL || children[i]->u64 != children[0]->u64)
        {
          identical = false;
          break;
        }
      }

      if(identical)
      {
        std::string ret = children[0]->GetValString() + ".";
        for(size_t i = 0; i < children.size(); i++)
          ret += 'x';
        return ret;
      }
    }

    std::string ret;
    if(type->type == SPVTypeData::eArray)
    {
      ret = type->baseType->GetName();
      ret += StringFormat::Fmt("[%u]", (uint32_t)children.size());
    }
    else
    {
      ret = type->GetName();
    }
    ret += "(";
    if(children.empty())
      ret += GetValString();
    for(size_t i = 0; i < children.size(); i++)
    {
      if(children[i] != NULL)
        ret += children[i]->GetIDName();
      else
        ret += "????";

      if(i + 1 < children.size())
      {
        ret += ", ";
        // put each array element on a different line, with some kind of
        // estimated indent (too complex with current blindly-append
        // scheme to match exactly)
        if(type->type == SPVTypeData::eArray)
          ret += "\n                        ";
      }
    }
    ret += ")";

    return ret;
  }
};

struct SPVVariable
{
  SPVVariable() : type(NULL), storage(spv::StorageClassUniformConstant), initialiser(NULL) {}
  SPVTypeData *type;
  spv::StorageClass storage;
  SPVConstant *initialiser;
};

struct SPVFlowControl
{
  SPVFlowControl() : selControl(spv::SelectionControlMaskNone), condition(NULL) {}
  union
  {
    spv::SelectionControlMask selControl;
    spv::LoopControlMask loopControl;
  };

  SPVInstruction *condition;

  // branch weights or switch cases
  std::vector<uint32_t> literals;

  // flow control can reference future IDs, so we index
  std::vector<uint32_t> targets;
};

struct SPVBlock
{
  SPVBlock() : mergeFlow(NULL), exitFlow(NULL) {}
  std::vector<SPVInstruction *> instructions;

  SPVInstruction *mergeFlow;
  SPVInstruction *exitFlow;
};

struct SPVFunction
{
  SPVFunction() : retType(NULL), funcType(NULL), control(spv::FunctionControlMaskNone) {}
  SPVTypeData *retType;
  SPVTypeData *funcType;
  std::vector<SPVInstruction *> arguments;

  spv::FunctionControlMask control;

  std::vector<SPVInstruction *> blocks;
  std::vector<SPVInstruction *> variables;
};

struct SPVInstruction
{
  SPVInstruction()
  {
    opcode = spv::OpNop;
    id = 0;

    ext = NULL;
    entry = NULL;
    op = NULL;
    flow = NULL;
    type = NULL;
    func = NULL;
    block = NULL;
    constant = NULL;
    var = NULL;

    line = -1;

    source.col = source.line = 0;
  }

  ~SPVInstruction()
  {
    SAFE_DELETE(ext);
    SAFE_DELETE(entry);
    SAFE_DELETE(op);
    SAFE_DELETE(flow);
    SAFE_DELETE(type);
    SAFE_DELETE(func);
    SAFE_DELETE(block);
    SAFE_DELETE(constant);
    SAFE_DELETE(var);
  }

  spv::Op opcode;
  uint32_t id;

  // line number in disassembly (used for stepping when debugging)
  int line;

  struct
  {
    std::string filename;
    uint32_t line;
    uint32_t col;
  } source;

  std::string str;

  const std::string &GetIDName()
  {
    if(str.empty())
    {
      if(opcode == spv::OpConstantNull)
        str = "null";
      else if(constant)
        str = constant->GetIDName();
      else
        str = DefaultIDName(id);
    }

    return str;
  }

  std::string Disassemble(const std::vector<SPVInstruction *> &ids, bool inlineOp)
  {
    switch(opcode)
    {
      case spv::OpUndef: return "UNDEFINED_VALUE";
      case spv::OpConstant:
      case spv::OpConstantTrue:
      case spv::OpConstantFalse:
      case spv::OpConstantComposite:
      case spv::OpVariable:
      case spv::OpFunctionParameter:
      case spv::OpSpecConstant:
      case spv::OpSpecConstantTrue:
      case spv::OpSpecConstantFalse:
      case spv::OpSpecConstantComposite:
      case spv::OpSpecConstantOp: return GetIDName();
      case spv::OpLabel:
      {
        RDCASSERT(!inlineOp);
        return StringFormat::Fmt("Label%u:", id);
      }
      case spv::OpReturn:
      {
        RDCASSERT(!inlineOp);
        return "return";
      }
      case spv::OpReturnValue:
      {
        RDCASSERT(!inlineOp);

        uint32_t retID = flow->targets[0];

        std::string arg;

        if(ids[retID] == NULL)
          arg = StringFormat::Fmt("<%u>", retID);
        else
          arg = ids[retID]->Disassemble(ids, true);

        return StringFormat::Fmt("return %s", arg.c_str());
      }
      case spv::OpBranch:
      {
        RDCASSERT(!inlineOp);
        return StringFormat::Fmt("goto Label%u", flow->targets[0]);
      }
      case spv::OpBranchConditional:
      {
        // we don't output the targets since that is handled specially

        std::string conditionStr;
        if(flow->condition->op == NULL || flow->condition->op->complexity < NEVER_INLINE_COMPLEXITY)
          conditionStr = flow->condition->Disassemble(ids, true);
        else
          conditionStr = flow->condition->GetIDName();

        if(flow->literals.empty())
          return conditionStr;

        uint32_t weightA = flow->literals[0];
        uint32_t weightB = flow->literals[1];

        float a = float(weightA) / float(weightA + weightB);
        float b = float(weightB) / float(weightA + weightB);

        a *= 100.0f;
        b *= 100.0f;

        return StringFormat::Fmt("%s [true: %.2f%%, false: %.2f%%]", conditionStr.c_str(), a, b);
      }
      case spv::OpSwitch:
      {
        return StringFormat::Fmt("switch(%s)", flow->condition->Disassemble(ids, true).c_str());
      }
      case spv::OpSelectionMerge:
      {
        RDCASSERT(!inlineOp);
        return StringFormat::Fmt("SelectionMerge Label%u%s", flow->targets[0],
                                 OptionalFlagString(flow->selControl).c_str());
      }
      case spv::OpLoopMerge:
      {
        RDCASSERT(!inlineOp);
        return StringFormat::Fmt("LoopMerge Label%u%s", flow->targets[0],
                                 OptionalFlagString(flow->loopControl).c_str());
      }
      case spv::OpStore:
      {
        RDCASSERT(op);

        // detect i++
        if(!inlineOp)
        {
          SPVInstruction *dstvar = op->arguments[0];
          if(op->arguments[1]->opcode == spv::OpIAdd)
          {
            SPVInstruction *srcvar = op->arguments[1]->op->arguments[0];
            SPVInstruction *addval = op->arguments[1]->op->arguments[1];
            if(srcvar->opcode == spv::OpLoad)
              srcvar = srcvar->op->arguments[0];

            if(dstvar == srcvar && addval->constant && addval->constant->type->IsBasicInt() &&
               addval->constant->u32 == 1)
              return dstvar->GetIDName() + "++";
          }
        }

        std::string dest, src;
        op->GetArg(ids, 0, dest);
        op->GetArg(ids, 1, src, false);

        // inlined only in function parameters, just return argument
        if(inlineOp)
          return src;

        char assignStr[] = " = ";

        if(op->arguments[1]->opcode == spv::OpCompositeInsert && (op->inlineArgs & 2))
          assignStr[0] = 0;

#if LOAD_STORE_CONSTRUCTORS
        return StringFormat::Fmt("Store(%s%s)%s%s", dest.c_str(),
                                 OptionalFlagString(op->access).c_str(), assignStr, src.c_str());
#else
        return StringFormat::Fmt("%s%s%s%s", dest.c_str(), OptionalFlagString(op->access).c_str(),
                                 assignStr, src.c_str());
#endif
      }
      case spv::OpCopyMemory:
      {
        RDCASSERT(!inlineOp && op);

        std::string dest, src;
        op->GetArg(ids, 0, dest);
        op->GetArg(ids, 1, src, false);

#if LOAD_STORE_CONSTRUCTORS
        return StringFormat::Fmt("Copy(%s%s) = Load(%s%s)", dest.c_str(),
                                 OptionalFlagString(op->access).c_str(), src.c_str(),
                                 OptionalFlagString(op->access).c_str());
#else
        return StringFormat::Fmt("%s%s = %s%s", dest.c_str(), OptionalFlagString(op->access).c_str(),
                                 src.c_str(), OptionalFlagString(op->access).c_str());
#endif
      }
      case spv::OpLoad:
      {
        RDCASSERT(op);

        std::string arg;
        op->GetArg(ids, 0, arg, false);

#if LOAD_STORE_CONSTRUCTORS
        if(inlineOp)
          return StringFormat::Fmt("Load(%s%s)", arg.c_str(), OptionalFlagString(op->access).c_str());

        return StringFormat::Fmt("%s %s = Load(%s%s)", op->type->GetName().c_str(),
                                 GetIDName().c_str(), arg.c_str(),
                                 OptionalFlagString(op->access).c_str());
#else
        if(inlineOp)
          return StringFormat::Fmt("%s%s", arg.c_str(), OptionalFlagString(op->access).c_str());

        return StringFormat::Fmt("%s %s = %s%s", op->type->GetName().c_str(), GetIDName().c_str(),
                                 arg.c_str(), OptionalFlagString(op->access).c_str());
#endif
      }
      case spv::OpPhi:
      case spv::OpCompositeConstruct:
      {
        RDCASSERT(op);

        std::string ret = "";

        if(!inlineOp)
          ret = StringFormat::Fmt("%s %s = ", op->type->GetName().c_str(), GetIDName().c_str());

        if(opcode == spv::OpPhi)
          ret += "Phi";
        else
          ret += op->type->GetName();
        ret += "(";

        bool allEqual = true;

        for(size_t i = 1; i < op->arguments.size(); i++)
        {
          if(op->arguments[i] != op->arguments[0])
          {
            allEqual = false;
            break;
          }
        }

        if(allEqual)
        {
          std::string arg0;
          op->GetArg(ids, 0, arg0, false);
          ret += arg0 + ")";
          return ret;
        }

        for(size_t i = 0; i < op->arguments.size(); i++)
        {
          bool added = false;

          // combine multiple vector compositeextract arguments together
          if(opcode == spv::OpCompositeConstruct && op->type->type == SPVTypeData::eVector &&
             op->arguments[i]->opcode == spv::OpCompositeExtract &&
             op->arguments[i]->op->arguments[0]->op &&
             op->type->type == op->arguments[i]->op->arguments[0]->op->type->type)
          {
            // we only combine if there are more than one argument that extract
            // from the same source, so begin==i, find out how many next arguments
            // there are
            size_t begin = i;
            size_t end = i;

            for(size_t j = i + 1; j < op->arguments.size(); j++)
            {
              // if argument j is an extract from the same source as argument i
              if(op->arguments[j]->opcode == spv::OpCompositeExtract &&
                 op->arguments[j]->op->arguments[0] == op->arguments[i]->op->arguments[0])
              {
                end = j;
              }
              else
              {
                break;
              }
            }

            // now combine them (this might be the trivial case of only 1 extraction
            // but we still want to inline that to avoid unnecessary temporaries
            {
              char swizzle[] = "xyzw";

              std::string swizzleString;

              for(size_t j = begin; j <= end; j++)
              {
                RDCASSERTMSG("Swizzle index >= 4", op->arguments[j]->op->literals[0] < 4,
                             op->arguments[j]->op->literals[0]);

                if(op->arguments[j]->op->literals[0] < 4)
                  swizzleString += swizzle[op->arguments[j]->op->literals[0]];
              }

              // if it ends up being the identity swizzle of the same size as the base, just insert
              // the base.
              if(swizzleString.length() < 4 &&
                 swizzleString.length() == op->arguments[i]->op->arguments[0]->op->type->vectorSize &&
                 !strncmp(swizzleString.c_str(), swizzle, swizzleString.length()))
              {
                std::string base;
                op->arguments[i]->op->GetArg(ids, 0, base, false);

                ret += base;
              }
              else
              {
                std::string base;
                op->arguments[i]->op->GetArg(ids, 0, base);

                ret += StringFormat::Fmt("%s.%s", base.c_str(), swizzleString.c_str());
              }

              added = true;

              // skip the arguments we combined together
              i += end - begin;
            }
          }

          if(!added)
          {
            std::string constituent;
            op->GetArg(ids, i, constituent, false);
            ret += constituent;
          }

          if(i + 1 < op->arguments.size())
            ret += ", ";
        }

        ret += ")";

        return ret;
      }
      case spv::OpVectorExtractDynamic:
      case spv::OpCompositeExtract:
      case spv::OpCompositeInsert:
      case spv::OpAccessChain:
      case spv::OpArrayLength:
      case spv::OpInBoundsAccessChain:
      {
        RDCASSERT(op);

        std::string composite;
        op->GetArg(ids, 0, composite);

        // unknown argument, we can't access chain it
        if(op->arguments[0]->var == NULL && op->arguments[0]->op == NULL)
        {
          std::string ret = "";

          if(!inlineOp)
            ret = StringFormat::Fmt("%s %s = ", op->type->GetName().c_str(), GetIDName().c_str());

          ret += composite + "....";

          return ret;
        }

        SPVTypeData *arg0type =
            op->arguments[0]->var ? op->arguments[0]->var->type : op->arguments[0]->op->type;

        RDCASSERT(arg0type);

        if(arg0type->type == SPVTypeData::ePointer)
          arg0type = arg0type->baseType;

        bool accessChain = (opcode == spv::OpAccessChain || opcode == spv::OpInBoundsAccessChain ||
                            opcode == spv::OpVectorExtractDynamic);

        size_t start = (accessChain ? 1 : 0);
        size_t count = (accessChain ? op->arguments.size() : op->literals.size());

        std::string accessString = "";

        for(size_t i = start; i < count; i++)
        {
          bool isConstant = false;
          uint32_t idx = 0;
          if(!accessChain)
          {
            idx = op->literals[i];
            isConstant = true;
          }
          else if(op->arguments[i]->constant)
          {
            RDCASSERT(op->arguments[i]->constant && op->arguments[i]->constant->type->IsBasicInt());
            idx = op->arguments[i]->constant->u32;
            isConstant = true;
          }

          if(!arg0type)
            break;

          if(arg0type->type == SPVTypeData::eStruct)
          {
            // Assuming you can't dynamically index into a structure
            RDCASSERT(isConstant);
            if(idx >= arg0type->children.size())
            {
              accessString += StringFormat::Fmt("._invalid_member%u", idx);
              break;
            }
            else
            {
              const rdcpair<SPVTypeData *, std::string> &child = arg0type->children[idx];
              if(child.second.empty())
                accessString += StringFormat::Fmt("._member%u", idx);
              else
                accessString += StringFormat::Fmt(".%s", child.second.c_str());
              arg0type = child.first;
            }
            continue;
          }
          else if(arg0type->type == SPVTypeData::eArray)
          {
            if(isConstant)
            {
              accessString += StringFormat::Fmt("[%u]", idx);
            }
            else
            {
              // dynamic indexing into this array
              std::string arg;
              op->GetArg(ids, i, arg);
              accessString += StringFormat::Fmt("[%s]", arg.c_str());
            }
            arg0type = arg0type->baseType;
            continue;
          }
          else if(arg0type->type == SPVTypeData::eMatrix)
          {
            if(isConstant)
            {
              accessString += StringFormat::Fmt("[%u]", idx);
            }
            else
            {
              // dynamic indexing into this array
              std::string arg;
              op->GetArg(ids, i, arg);
              accessString += StringFormat::Fmt("[%s]", arg.c_str());
            }

            // fall through to vector if we have another index
            if(i == count - 1)
              break;

            i++;

            if(!accessChain)
            {
              idx = op->literals[i];
            }
            else
            {
              // if it's a constant index, treat it like a vector access
              if(op->arguments[i]->constant && op->arguments[i]->constant->type->IsBasicInt())
              {
                idx = op->arguments[i]->constant->u32;
              }
              else
              {
                // otherwise we have to treat it as another dynamic index, which is valid

                std::string arg;
                op->GetArg(ids, i, arg);
                accessString += StringFormat::Fmt("[%s]", arg.c_str());

                continue;
              }
            }
          }

          // vector (or matrix + extra)
          if(opcode == spv::OpVectorExtractDynamic)
          {
            std::string arg;
            op->GetArg(ids, 1, arg);
            accessString += StringFormat::Fmt("[%s]", arg.c_str());
          }
          else
          {
            char swizzle[] = "xyzw";
            if(idx < 4)
              accessString += StringFormat::Fmt(".%c", swizzle[idx]);
            else
              accessString += StringFormat::Fmt("._%u", idx);

            // must be the last index, we're down to scalar granularity
            arg0type = NULL;
            RDCASSERT(i == count - 1);
          }
        }

        std::string ret = "";

        if(opcode == spv::OpCompositeInsert)
        {
          std::string insertObj;
          op->GetArg(ids, 1, insertObj);

          // if we've been inlined, it means that there is a store of the result of
          // this composite insert, to the same composite that we are cloning (first
          // argument). If so, we can just leave the access and object assignment
          if(inlineOp)
          {
            ret = StringFormat::Fmt("%s = %s", accessString.c_str(), insertObj.c_str());
          }
          else
          {
            ret = StringFormat::Fmt("%s %s = %s; %s%s = %s", op->type->GetName().c_str(),
                                    GetIDName().c_str(), composite.c_str(), GetIDName().c_str(),
                                    accessString.c_str(), insertObj.c_str());
          }
        }
        else
        {
          if(!inlineOp)
            ret = StringFormat::Fmt("%s %s = ", op->type->GetName().c_str(), GetIDName().c_str());

          ret += composite + accessString;

          if(opcode == spv::OpArrayLength)
            ret += ".length()";
        }

        return ret;
      }
      case spv::OpExtInst:
      {
        RDCASSERT(op);

        std::string ret = "";

        if(!inlineOp)
          ret = StringFormat::Fmt("%s %s = ", op->type->GetName().c_str(), GetIDName().c_str());

#if USE_CANONICAL_EXT_INST_NAMES
        ret += op->arguments[0]->ext->setname + "::";
        const char **names = op->arguments[0]->ext->canonicalNames;
#else
        const char **names = op->arguments[0]->ext->friendlyNames;
#endif
        if(names)
        {
          ret += names[op->literals[0]];
        }
        else
        {
#if !USE_CANONICAL_EXT_INST_NAMES
          ret += op->arguments[0]->ext->setname + "::";
#endif
          ret += StringFormat::Fmt("op%u", op->literals[0]);
        }

        ret += "(";

        for(size_t i = 1; i < op->arguments.size(); i++)
        {
          std::string arg;
          op->GetArg(ids, i, arg, false);

          ret += arg;
          if(i + 1 < op->arguments.size())
            ret += ", ";
        }

        ret += ")";

        return ret;
      }
      case spv::OpImageTexelPointer:
      {
        RDCASSERT(!inlineOp && op);

        std::string image, coord, sample;
        op->GetArg(ids, 0, image);
        op->GetArg(ids, 1, coord, false);
        op->GetArg(ids, 2, sample, false);

        std::string ret =
            StringFormat::Fmt("%s %s = ImageTexelPointer(%s, %s, %s)", op->type->GetName().c_str(),
                              GetIDName().c_str(), image.c_str(), coord.c_str(), sample.c_str());

        return ret;
      }
      // Most of the following are just of the form OpcodeName(arg1, arg2, arg3..)
      // like a function call. Operations can very by return type (e.g. image
      // vs imagesparse opcodes) without changing their disassembly
      case spv::OpImageSampleImplicitLod:
      case spv::OpImageSampleExplicitLod:
      case spv::OpImageSampleDrefImplicitLod:
      case spv::OpImageSampleDrefExplicitLod:
      case spv::OpImageSampleProjImplicitLod:
      case spv::OpImageSampleProjExplicitLod:
      case spv::OpImageSampleProjDrefImplicitLod:
      case spv::OpImageSampleProjDrefExplicitLod:
      case spv::OpImageSparseSampleImplicitLod:
      case spv::OpImageSparseSampleExplicitLod:
      case spv::OpImageSparseSampleDrefImplicitLod:
      case spv::OpImageSparseSampleDrefExplicitLod:
      case spv::OpImageSparseSampleProjImplicitLod:
      case spv::OpImageSparseSampleProjExplicitLod:
      case spv::OpImageSparseSampleProjDrefImplicitLod:
      case spv::OpImageSparseSampleProjDrefExplicitLod:
      case spv::OpImageFetch:
      case spv::OpImageGather:
      case spv::OpImageDrefGather:
      case spv::OpImageRead:
      case spv::OpImageWrite:
      case spv::OpImageSparseFetch:
      case spv::OpImageSparseGather:
      case spv::OpImageSparseDrefGather:
      case spv::OpImageSparseRead:
      case spv::OpAtomicStore:
      case spv::OpAtomicExchange:
      case spv::OpAtomicCompareExchange:
      case spv::OpAtomicIIncrement:
      case spv::OpAtomicIDecrement:
      case spv::OpAtomicIAdd:
      case spv::OpAtomicISub:
      case spv::OpAtomicSMin:
      case spv::OpAtomicUMin:
      case spv::OpAtomicSMax:
      case spv::OpAtomicUMax:
      case spv::OpAtomicAnd:
      case spv::OpAtomicOr:
      case spv::OpAtomicXor:
      case spv::OpConvertFToS:
      case spv::OpConvertFToU:
      case spv::OpConvertUToF:
      case spv::OpConvertSToF:
      case spv::OpQuantizeToF16:
      case spv::OpFConvert:
      case spv::OpUConvert:
      case spv::OpSConvert:
      case spv::OpBitcast:
      case spv::OpBitReverse:
      case spv::OpBitCount:
      case spv::OpAny:
      case spv::OpAll:
      case spv::OpIsNan:
      case spv::OpIsInf:
      case spv::OpOuterProduct:
      case spv::OpTranspose:
      case spv::OpCopyObject:
      case spv::OpDPdx:
      case spv::OpDPdy:
      case spv::OpFwidth:
      case spv::OpDPdxFine:
      case spv::OpDPdyFine:
      case spv::OpFwidthFine:
      case spv::OpDPdxCoarse:
      case spv::OpDPdyCoarse:
      case spv::OpFwidthCoarse:
      case spv::OpImageSparseTexelsResident:
      case spv::OpImage:
      case spv::OpSampledImage:
      case spv::OpImageQuerySizeLod:
      case spv::OpImageQuerySize:
      case spv::OpImageQueryLod:
      case spv::OpImageQueryLevels:
      case spv::OpImageQuerySamples:
      case spv::OpFunctionCall:
      {
        RDCASSERT(op);

        std::string ret = "";

        if(!inlineOp && op->type && op->type->type != SPVTypeData::eVoid &&
           opcode != spv::OpAtomicStore)
          ret = StringFormat::Fmt("%s %s = ", op->type->GetName().c_str(), GetIDName().c_str());

        size_t numArgs = op->arguments.size();

        if(opcode == spv::OpFunctionCall)
        {
          ret += ids[op->funcCall]->GetIDName() + "(";
        }
        else if(opcode == spv::OpBitcast)
        {
          ret += "Bitcast<" + op->type->GetName() + ">(";
        }
        else if(opcode == spv::OpImageGather)
        {
          // last argument is the component, reads better to have this
          // as part of the operation (if it was a constant instead of
          // an ID
          std::string arg;
          op->GetArg(ids, numArgs - 1, arg);
          ret += "ImageGather[" + arg + "](";

          numArgs--;
        }
        else
        {
          ret += ToStr(opcode) + "(";
        }

        for(size_t i = 0; i < numArgs; i++)
        {
          std::string arg;
          op->GetArg(ids, i, arg, false);

          if(op->im.bias == op->arguments[i])
            ret += "Bias = ";
          else if(op->im.constOffset == op->arguments[i])
            ret += "ConstOffset = ";
          else if(op->im.dx == op->arguments[i])
            ret += "Gradients = <";
          else if(op->im.gatherOffsets == op->arguments[i])
            ret += "GatherOffsets = ";
          else if(op->im.lod == op->arguments[i])
            ret += "LOD = ";
          else if(op->im.minLod == op->arguments[i])
            ret += "MinLOD = ";
          else if(op->im.offset == op->arguments[i])
            ret += "Offset = ";
          else if(op->im.sampleIdx == op->arguments[i])
            ret += "SampleIdx = ";

          ret += arg;

          if(op->im.dy == op->arguments[i])
            ret += ">";    // closes < above when processing dx

          if(i + 1 < op->arguments.size())
            ret += ", ";
        }

        // for atomic operations, print the execution scope and memory semantics
        switch(opcode)
        {
          case spv::OpAtomicStore:
          case spv::OpAtomicExchange:
          case spv::OpAtomicIIncrement:
          case spv::OpAtomicIDecrement:
          case spv::OpAtomicIAdd:
          case spv::OpAtomicISub:
          case spv::OpAtomicSMin:
          case spv::OpAtomicUMin:
          case spv::OpAtomicSMax:
          case spv::OpAtomicUMax:
          case spv::OpAtomicAnd:
          case spv::OpAtomicOr:
          case spv::OpAtomicXor:
            ret += StringFormat::Fmt(", Scope=%s, Semantics=%s", ToStr(op->scope).c_str(),
                                     ToStr(op->semantics).c_str());
            break;
          case spv::OpAtomicCompareExchange:
            ret += StringFormat::Fmt(", Scope=%s, Semantics=(equal: %s unequal: %s)",
                                     ToStr(op->scope).c_str(), ToStr(op->semantics).c_str(),
                                     ToStr(op->semanticsUnequal).c_str());
            break;
          default: break;
        }

        ret += ")";

        return ret;
      }
      case spv::OpEmitVertex:
      case spv::OpEmitStreamVertex:
      case spv::OpEndPrimitive:
      case spv::OpEndStreamPrimitive: { return ToStr(opcode) + "()";
      }
      case spv::OpControlBarrier:
      {
        return StringFormat::Fmt("%s(Execution Scope=%s, Memory Scope=%s, Semantics=%s)",
                                 ToStr(opcode).c_str(), ToStr(op->scope).c_str(),
                                 ToStr(op->scopeMemory).c_str(), ToStr(op->semantics).c_str());
      }
      case spv::OpMemoryBarrier:
      {
        return StringFormat::Fmt("%s(Scope=%s, Semantics=%s)", ToStr(opcode).c_str(),
                                 ToStr(op->scope).c_str(), ToStr(op->semantics).c_str());
      }
      case spv::OpVectorShuffle:
      {
        RDCASSERT(op);

        std::string ret = "";

        if(!inlineOp)
          ret = StringFormat::Fmt("%s %s = ", op->type->GetName().c_str(), GetIDName().c_str());

        SPVTypeData *vec1type = NULL;
        SPVTypeData *vec2type = NULL;

        if(op->arguments[0]->constant)
          vec1type = op->arguments[0]->constant->type;
        if(op->arguments[0]->var)
          vec1type = op->arguments[0]->var->type;
        if(op->arguments[0]->op)
          vec1type = op->arguments[0]->op->type;

        if(op->arguments[1]->constant)
          vec2type = op->arguments[1]->constant->type;
        if(op->arguments[1]->var)
          vec2type = op->arguments[1]->var->type;
        if(op->arguments[1]->op)
          vec2type = op->arguments[1]->op->type;

        // can't gracefully handle unknown arguments here
        if(op->arguments[0]->opcode == spv::OpUnknown ||
           op->arguments[0]->opcode == spv::OpUnknown || vec1type == NULL || vec2type == NULL)
        {
          ret += StringFormat::Fmt("VectorShuffle(%s, %s)",
                                   op->arguments[0]->Disassemble(ids, true).c_str(),
                                   op->arguments[1]->Disassemble(ids, true).c_str());
          return ret;
        }

        RDCASSERT(vec1type->type == SPVTypeData::eVector && vec2type->type == SPVTypeData::eVector);

        uint32_t maxShuffle = 0;
        for(size_t i = 0; i < op->literals.size(); i++)
        {
          uint32_t s = op->literals[i];
          if(s >= vec1type->vectorSize)
            s -= vec1type->vectorSize;
          maxShuffle = RDCMAX(maxShuffle, s);
        }

        // if the vectors are identical, we're just doing a swizzle on one vector
        // so there's no need for a surrounding constructor syntax
        if(op->arguments[0] != op->arguments[1])
          ret += op->type->GetName() + "(";

        // sane path for 4-vectors or less
        if(maxShuffle < 4)
        {
          char swizzle[] = "xyzw_";

          int lastvec = -1;
          for(size_t i = 0; i < op->literals.size(); i++)
          {
            int vec = 0;
            uint32_t s = op->literals[i];
            if(s == 0xFFFFFFFF)
            {
              // undefined component
              s = 4;
            }
            else if(s >= vec1type->vectorSize)
            {
              s -= vec1type->vectorSize;
              vec = 1;

              // just for sanity, pretend it indexed into the first vector
              // when they're identical - as we don't have constructor text
              if(op->arguments[0] == op->arguments[1])
                vec = 0;
            }

            if(vec != lastvec)
            {
              lastvec = vec;
              if(i > 0)
                ret += ", ";
              std::string arg;
              op->GetArg(ids, vec, arg);

              ret += arg;
              ret += ".";
            }

            ret += swizzle[s];
          }
        }
        else
        {
          RDCERR("Not disassembling a shuffle of a vector larger than 4 wide!");
        }

        if(op->arguments[0] != op->arguments[1])
          ret += ")";

        return ret;
      }
      case spv::OpFNegate:
      case spv::OpSNegate:
      case spv::OpNot:
      case spv::OpLogicalNot:
      {
        // unary math operation
        RDCASSERT(op);

        char c = '?';
        switch(opcode)
        {
          case spv::OpFNegate:
          case spv::OpSNegate: c = '-'; break;
          case spv::OpNot: c = '~'; break;
          case spv::OpLogicalNot: c = '!'; break;
          default: break;
        }

        std::string a;
        op->GetArg(ids, 0, a);

        if(inlineOp)
          return StringFormat::Fmt("%c%s", c, a.c_str());

        return StringFormat::Fmt("%s %s = %c%s", op->type->GetName().c_str(), GetIDName().c_str(),
                                 c, a.c_str());
      }
      case spv::OpIAdd:
      case spv::OpFAdd:
      case spv::OpISub:
      case spv::OpFSub:
      case spv::OpIMul:
      case spv::OpFMul:
      case spv::OpFDiv:
      case spv::OpUDiv:
      case spv::OpSDiv:
      case spv::OpFMod:
      case spv::OpUMod:
      case spv::OpSMod:
      case spv::OpFRem:
      case spv::OpSRem:
      case spv::OpVectorTimesScalar:
      case spv::OpMatrixTimesScalar:
      case spv::OpMatrixTimesVector:
      case spv::OpVectorTimesMatrix:
      case spv::OpMatrixTimesMatrix:
      case spv::OpIEqual:
      case spv::OpINotEqual:
      case spv::OpSLessThan:
      case spv::OpSLessThanEqual:
      case spv::OpSGreaterThan:
      case spv::OpSGreaterThanEqual:
      case spv::OpULessThan:
      case spv::OpULessThanEqual:
      case spv::OpUGreaterThan:
      case spv::OpUGreaterThanEqual:
      case spv::OpFOrdEqual:
      case spv::OpFOrdNotEqual:
      case spv::OpFOrdLessThan:
      case spv::OpFOrdLessThanEqual:
      case spv::OpFOrdGreaterThan:
      case spv::OpFOrdGreaterThanEqual:
      case spv::OpFUnordEqual:
      case spv::OpFUnordNotEqual:
      case spv::OpFUnordLessThan:
      case spv::OpFUnordLessThanEqual:
      case spv::OpFUnordGreaterThan:
      case spv::OpFUnordGreaterThanEqual:
      case spv::OpLogicalAnd:
      case spv::OpLogicalOr:
      case spv::OpLogicalEqual:
      case spv::OpLogicalNotEqual:
      case spv::OpBitwiseAnd:
      case spv::OpBitwiseOr:
      case spv::OpBitwiseXor:
      case spv::OpShiftLeftLogical:
      case spv::OpShiftRightLogical:
      case spv::OpShiftRightArithmetic:
      {
        // binary math operation
        RDCASSERT(op);

        char opstr[4] = {'?', 0, 0, 0};
        switch(opcode)
        {
          case spv::OpIAdd:
          case spv::OpFAdd: opstr[0] = '+'; break;
          case spv::OpISub:
          case spv::OpFSub: opstr[0] = '-'; break;
          case spv::OpIMul:
          case spv::OpFMul:
          case spv::OpVectorTimesScalar:
          case spv::OpMatrixTimesScalar:
          case spv::OpMatrixTimesVector:
          case spv::OpVectorTimesMatrix:
          case spv::OpMatrixTimesMatrix: opstr[0] = '*'; break;
          case spv::OpSLessThan:
          case spv::OpULessThan:
          case spv::OpFOrdLessThan:
          case spv::OpFUnordLessThan: opstr[0] = '<'; break;
          case spv::OpSLessThanEqual:
          case spv::OpULessThanEqual:
          case spv::OpFOrdLessThanEqual:
          case spv::OpFUnordLessThanEqual:
            opstr[0] = '<';
            opstr[1] = '=';
            break;
          case spv::OpSGreaterThan:
          case spv::OpUGreaterThan:
          case spv::OpFOrdGreaterThan:
          case spv::OpFUnordGreaterThan: opstr[0] = '>'; break;
          case spv::OpSGreaterThanEqual:
          case spv::OpUGreaterThanEqual:
          case spv::OpFOrdGreaterThanEqual:
          case spv::OpFUnordGreaterThanEqual:
            opstr[0] = '>';
            opstr[1] = '=';
            break;
          case spv::OpFDiv:
          case spv::OpUDiv:
          case spv::OpSDiv: opstr[0] = '/'; break;
          case spv::OpFMod:
          case spv::OpUMod:
          case spv::OpSMod: opstr[0] = '%'; break;
          case spv::OpFRem:
          case spv::OpSRem:
            opstr[0] = 'r';
            opstr[1] = 'e';
            opstr[2] = 'm';
            break;
          case spv::OpLogicalAnd: opstr[0] = opstr[1] = '&'; break;
          case spv::OpLogicalOr: opstr[0] = opstr[1] = '|'; break;
          case spv::OpBitwiseAnd: opstr[0] = '&'; break;
          case spv::OpBitwiseOr: opstr[0] = '|'; break;
          case spv::OpBitwiseXor: opstr[0] = '^'; break;
          case spv::OpIEqual:
          case spv::OpLogicalEqual:
          case spv::OpFOrdEqual:
          case spv::OpFUnordEqual:
            opstr[0] = '=';
            opstr[1] = '=';
            break;
          case spv::OpINotEqual:
          case spv::OpLogicalNotEqual:
          case spv::OpFOrdNotEqual:
          case spv::OpFUnordNotEqual:
            opstr[0] = '!';
            opstr[1] = '=';
            break;
          case spv::OpShiftLeftLogical:
            opstr[0] = '<';
            opstr[1] = '<';
            break;
          // essentially unsigned vs signed shift right, so display without
          // distinction
          case spv::OpShiftRightLogical:
          case spv::OpShiftRightArithmetic:
            opstr[0] = '>';
            opstr[1] = '>';
            break;
          default: RDCERR("Unhandled bin math op in switch"); break;
        }

        std::string a, b;
        op->GetArg(ids, 0, a);
        op->GetArg(ids, 1, b);

        if(inlineOp)
          return StringFormat::Fmt("%s %s %s", a.c_str(), opstr, b.c_str());

        return StringFormat::Fmt("%s %s = %s %s %s", op->type->GetName().c_str(),
                                 GetIDName().c_str(), a.c_str(), opstr, b.c_str());
      }
      case spv::OpDot:
      {
        // binary math function
        RDCASSERT(op);

        std::string a, b;
        op->GetArg(ids, 0, a, false);
        op->GetArg(ids, 1, b, false);

        if(inlineOp)
          return StringFormat::Fmt("%s(%s, %s)", ToStr(opcode).c_str(), a.c_str(), b.c_str());

        return StringFormat::Fmt("%s %s = %s(%s, %s)", op->type->GetName().c_str(),
                                 GetIDName().c_str(), ToStr(opcode).c_str(), a.c_str(), b.c_str());
      }
      case spv::OpSelect:
      {
        RDCASSERT(op);

        std::string a, b, c;
        op->GetArg(ids, 0, a, false);
        op->GetArg(ids, 1, b, false);
        op->GetArg(ids, 2, c, false);

        if(inlineOp)
          return StringFormat::Fmt("(%s) ? (%s) : (%s)", a.c_str(), b.c_str(), c.c_str());

        return StringFormat::Fmt("%s %s = (%s) ? (%s) : (%s)", op->type->GetName().c_str(),
                                 GetIDName().c_str(), a.c_str(), b.c_str(), c.c_str());
      }
      default: break;
    }

    if(opcode == spv::OpUnknown)
    {
      // we don't know where this ID came from, this is a dummy op
      return "UnknownOp(" + GetIDName() + ")";
    }

    // fallback for operations that we don't disassemble
    std::string ret = "!!";

    if(!str.empty() || id != 0)
      ret += GetIDName() + " <= ";

    ret = ToStr(opcode) + "(";
    for(size_t a = 0; op && a < op->arguments.size(); a++)
    {
      ret += op->arguments[a]->GetIDName();
      if(a + 1 < op->arguments.size())
        ret += ", ";
    }
    ret += ")";

    return ret;
  }

  std::vector<SPVDecoration> decorations;

  // zero or one of these pointers might be set
  SPVExtInstSet *ext;       // this ID is an extended instruction set
  SPVEntryPoint *entry;     // this ID is an entry point
  SPVOperation *op;         // this ID is the result of an operation
  SPVFlowControl *flow;     // this is a flow control operation (no ID)
  SPVTypeData *type;        // this ID names a type
  SPVFunction *func;        // this ID names a function
  SPVBlock *block;          // this is the ID of a label
  SPVConstant *constant;    // this ID is a constant value
  SPVVariable *var;         // this ID is a variable
};

std::string SPVTypeData::DeclareVariable(const std::vector<SPVDecoration> &vardecorations,
                                         const std::string &varName)
{
  std::string ret = "";

  const SPVDecoration *builtin = NULL;

  for(size_t d = 0; d < vardecorations.size(); d++)
  {
    if(vardecorations[d].decoration == spv::DecorationBuiltIn)
    {
      builtin = &vardecorations[d];
      continue;
    }
    std::string decorationStr = vardecorations[d].Str();
    if(!decorationStr.empty())
      ret += decorationStr + " ";
  }

  std::string arraySizeStr;

  if(type == ePointer && baseType->type == eArray)
  {
    if(baseType->arraySize == ~0U)
      arraySizeStr.clear();
    else if(baseType->arraySizeConst)
      arraySizeStr = baseType->arraySizeConst->GetIDName();
    else
      arraySizeStr = StringFormat::Fmt("%u", baseType->arraySize);

    ret += StringFormat::Fmt("%s* %s[%s]", baseType->baseType->GetName().c_str(), varName.c_str(),
                             arraySizeStr.c_str());
  }
  else if(type == eArray)
  {
    std::string arraySuffix;

    SPVTypeData *eltype = baseType;
    while(eltype->type == eArray)
    {
      arraySizeStr.clear();

      if(eltype->arraySize == ~0U)
        arraySizeStr.clear();
      else if(eltype->arraySizeConst)
        arraySizeStr = eltype->arraySizeConst->GetIDName();
      else
        arraySizeStr = StringFormat::Fmt("%u", eltype->arraySize);

      arraySuffix += StringFormat::Fmt("[%s]", arraySizeStr.c_str());
      eltype = eltype->baseType;
    }

    arraySizeStr.clear();

    if(arraySize == ~0U)
      arraySizeStr.clear();
    else if(arraySizeConst)
      arraySizeStr = arraySizeConst->GetIDName();
    else
      arraySizeStr = StringFormat::Fmt("%u", arraySize);

    ret += StringFormat::Fmt("%s %s[%s]%s", eltype->GetName().c_str(), varName.c_str(),
                             arraySizeStr.c_str(), arraySuffix.c_str());
  }
  else
  {
    ret += StringFormat::Fmt("%s %s", GetName().c_str(), varName.c_str());
  }

  if(builtin)
    ret += " = " + ToStr((spv::BuiltIn)builtin->val);

  return ret;
}

void SPVOperation::GetArg(const std::vector<SPVInstruction *> &ids, size_t idx, std::string &arg,
                          bool bracketArgumentsIfNeeded)
{
  if(inlineArgs & (1 << idx))
  {
    arg = arguments[idx]->Disassemble(ids, true);

    if(bracketArgumentsIfNeeded)
    {
      // skip past any inlined load(store())
      SPVInstruction *instr = arguments[idx];

      if(instr->opcode == spv::OpLoad && instr->op->arguments[0]->opcode == spv::OpStore)
      {
        instr = instr->op->arguments[0]->op->arguments[1];
      }

      // add brackets if needed
      if(instr->op && instr->op->mathop)
        arg = "(" + arg + ")";
    }
  }
  else
  {
    arg = arguments[idx]->GetIDName();
  }
}

static bool IsUnmodified(SPVFunction *func, SPVInstruction *from, SPVInstruction *to)
{
  // if it's not a variable (e.g. constant or something), just return true,
  // it's pure.
  if(from->op == NULL)
    return true;

  // if we're looking at a load of a variable, ensure that it's pure
  if(from->opcode == spv::OpLoad && from->op->arguments[0]->var)
  {
    SPVInstruction *var = from->op->arguments[0];

    bool looking = false;
    bool done = false;

    for(size_t b = 0; b < func->blocks.size(); b++)
    {
      SPVInstruction *block = func->blocks[b];

      for(size_t i = 0; i < block->block->instructions.size(); i++)
      {
        SPVInstruction *instr = block->block->instructions[i];
        if(instr == from)
        {
          looking = true;
        }
        else if(instr == to)
        {
          looking = false;
          done = true;
          break;
        }
        else if(looking && instr->opcode == spv::OpStore && instr->op->arguments[0] == var)
        {
          return false;
        }
      }

      if(done)
        break;
    }

    return true;
  }

  // hack - anything over 3 levels of recursion and we just bail rather than checking further.
  static int recurse = 0;

  if(recurse > 3)
    return false;

  recurse++;

  // otherwise, recurse
  bool ret = true;

  for(size_t i = 0; i < from->op->arguments.size(); i++)
  {
    if(from->opcode == spv::OpStore && i == 0)
      continue;

    // this operation is pure if all of its arguments are pure up to the point
    // of use
    ret &= IsUnmodified(func, from->op->arguments[i], to);
  }

  recurse--;
  return ret;
}

SPVModule::SPVModule()
{
  moduleVersion.major = moduleVersion.minor = 0;
  generator = 0;
  sourceVer = 0;
  sourceLang = spv::SourceLanguageUnknown;
}

SPVModule::~SPVModule()
{
  for(size_t i = 0; i < operations.size(); i++)
    delete operations[i];
  operations.clear();
}

SPVInstruction *SPVModule::GetByID(uint32_t id)
{
  if(ids[id])
    return ids[id];

  // if there's an unrecognised instruction (e.g. from an extension) that generates
  // an ID, it won't be in our list so we have to add a dummy instruction for it
  RDCWARN("Expected to find ID %u but didn't - returning dummy instruction", id);

  operations.push_back(new SPVInstruction());
  SPVInstruction &op = *operations.back();
  op.opcode = spv::OpUnknown;
  op.id = id;
  ids[id] = &op;

  return &op;
}

void FindFirstInstructionUse(SPVInstruction *op, SPVInstruction *search, SPVInstruction **result)
{
  if(op->op == NULL)
    return;

  for(size_t a = 0; a < op->op->arguments.size(); a++)
  {
    if(op->op->arguments[a] == search)
    {
      *result = op;
      return;
    }

    // recurse into the operation this argument might have inlined
    if(op->op->inlineArgs & (1 << a))
    {
      FindFirstInstructionUse(op->op->arguments[a], search, result);

      // if we found when recursing, exit
      if(*result)
        return;
    }
  }
}

std::string SPVModule::Disassemble(const std::string &entryPoint)
{
  std::string retDisasm = "";

  // TODO filter to only functions/resources used by entryPoint

  retDisasm = StringFormat::Fmt("SPIR-V %u.%u:\n\n", moduleVersion.major, moduleVersion.minor);

  if(moduleVersion.major != 1 || moduleVersion.minor != 0)
  {
    return retDisasm + "Unsupported version";
  }

  GeneratorID *gen = NULL;

  uint32_t toolid = (generator & 0xffff0000) >> 16;
  uint32_t version = (generator & 0xffff);

  for(size_t i = 0; i < ARRAY_COUNT(KnownGenerators); i++)
    if(KnownGenerators[i].toolid == toolid)
      gen = &KnownGenerators[i];

  if(gen)
    retDisasm += StringFormat::Fmt("%s from %s (%s) - version 0x%04x\n", gen->tool, gen->vendor,
                                   gen->comment, version);
  else
    retDisasm += StringFormat::Fmt("Generator not recognised: %08x\n", generator);
  retDisasm += StringFormat::Fmt("IDs up to {%u}\n", (uint32_t)ids.size());

  retDisasm += "\n";

  retDisasm += StringFormat::Fmt("Source is %s %u\n", ToStr(sourceLang).c_str(), sourceVer);
  for(size_t s = 0; s < sourceexts.size(); s++)
    retDisasm += StringFormat::Fmt(" + %s\n", sourceexts[s]->str.c_str());

  if(!sourceFiles.empty())
    retDisasm += "Source files:\n";
  for(size_t s = 0; s < sourceFiles.size(); s++)
    retDisasm += "    " + sourceFiles[s].first + "\n";

  retDisasm += "\n";

  if(!extensions.empty())
  {
    retDisasm += "SPIR-V Extensions:";
    for(size_t e = 0; e < extensions.size(); e++)
      retDisasm += " " + extensions[e];
    retDisasm += "\n";
  }

  retDisasm += "Capabilities:";
  for(size_t c = 0; c < capabilities.size(); c++)
    retDisasm += StringFormat::Fmt(" %s", ToStr(capabilities[c]).c_str());
  retDisasm += "\n";

  for(size_t i = 0; i < entries.size(); i++)
  {
    RDCASSERT(entries[i]->entry);
    uint32_t func = entries[i]->entry->func;
    RDCASSERT(ids[func]);
    retDisasm += StringFormat::Fmt("Entry point '%s' (%s)\n", ids[func]->str.c_str(),
                                   ToStr(entries[i]->entry->model).c_str());

    for(size_t m = 0; m < entries[i]->entry->modes.size(); m++)
    {
      SPVExecutionMode &mode = entries[i]->entry->modes[m];
      retDisasm += StringFormat::Fmt("            %s", ToStr(mode.mode).c_str());
      if(mode.mode == spv::ExecutionModeInvocations || mode.mode == spv::ExecutionModeOutputVertices)
        retDisasm += StringFormat::Fmt(" = %u", mode.x);
      if(mode.mode == spv::ExecutionModeLocalSize || mode.mode == spv::ExecutionModeLocalSizeHint)
        retDisasm += StringFormat::Fmt(" = <%u, %u, %u>", mode.x, mode.y, mode.z);
      if(mode.mode == spv::ExecutionModeVecTypeHint)
      {
        uint16_t dataType = (mode.x & 0xffff);
        uint16_t numComps = (mode.y >> 16) & 0xffff;
        switch(dataType)
        {
          // 0 represents an 8-bit integer value.
          case 0:
            retDisasm += StringFormat::Fmt(" = byte%u", numComps);
            break;
          // 1 represents a 16-bit integer value.
          case 1:
            retDisasm += StringFormat::Fmt(" = short%u", numComps);
            break;
          // 2 represents a 32-bit integer value.
          case 2:
            retDisasm += StringFormat::Fmt(" = int%u", numComps);
            break;
          // 3 represents a 64-bit integer value.
          case 3:
            retDisasm += StringFormat::Fmt(" = longlong%u", numComps);
            break;
          // 4 represents a 16-bit float value.
          case 4:
            retDisasm += StringFormat::Fmt(" = half%u", numComps);
            break;
          // 5 represents a 32-bit float value.
          case 5:
            retDisasm += StringFormat::Fmt(" = float%u", numComps);
            break;
          // 6 represents a 64-bit float value.
          case 6:
            retDisasm += StringFormat::Fmt(" = double%u", numComps);
            break;
          // ...
          default: retDisasm += StringFormat::Fmt(" = invalid%u", numComps); break;
        }
      }

      retDisasm += "\n";
    }
  }

  retDisasm += "\n";

  for(size_t i = 0; i < structs.size(); i++)
  {
    retDisasm += StringFormat::Fmt("struct %s {\n", structs[i]->type->GetName().c_str());
    for(size_t c = 0; c < structs[i]->type->children.size(); c++)
    {
      auto member = structs[i]->type->children[c];

      std::string varName = member.second;

      if(varName.empty())
        varName = StringFormat::Fmt("_member%u", c);

      retDisasm += StringFormat::Fmt(
          "  %s;\n",
          member.first->DeclareVariable(structs[i]->type->childDecorations[c], varName).c_str());
    }
    retDisasm += StringFormat::Fmt("}; // struct %s\n\n", structs[i]->type->GetName().c_str());
  }

  for(size_t i = 0; i < globals.size(); i++)
  {
    RDCASSERT(globals[i]->var && globals[i]->var->type);

    // if name is set to blank, inherit it from the underlying type
    // we set this to the variable name, so it can be used in subsequent ops
    // referring to this global.
    if(globals[i]->str.empty())
    {
      if(globals[i]->var && !globals[i]->var->type->name.empty())
        globals[i]->str =
            StringFormat::Fmt("%s_%u", globals[i]->var->type->name.c_str(), globals[i]->id);
      else if(globals[i]->var && globals[i]->var->type->type == SPVTypeData::ePointer &&
              !globals[i]->var->type->baseType->name.empty())
        globals[i]->str = StringFormat::Fmt("%s_%u", globals[i]->var->type->baseType->name.c_str(),
                                            globals[i]->id);
    }

    std::string varName = globals[i]->str;
    retDisasm += StringFormat::Fmt(
        "%s %s;\n", ToStr(globals[i]->var->storage).c_str(),
        globals[i]->var->type->DeclareVariable(globals[i]->decorations, varName).c_str());
  }

  for(size_t i = 0; i < specConstants.size(); i++)
  {
    RDCASSERT(specConstants[i]->constant && specConstants[i]->constant->type);

    uint32_t specId = ~0U;

    for(size_t d = 0; d < specConstants[i]->decorations.size(); d++)
    {
      if(specConstants[i]->decorations[d].decoration == spv::DecorationSpecId)
      {
        specId = specConstants[i]->decorations[d].val;
        break;
      }
    }

    if(specId == ~0U)
    {
      RDCERR("Couldn't find specialisation index for spec constant");
      continue;
    }

    std::string varName = specConstants[i]->str;
    retDisasm += StringFormat::Fmt(
        "%s = Specialize<ID %u>(%s);\n",
        specConstants[i]->constant->type->DeclareVariable(specConstants[i]->decorations, varName).c_str(),
        specId, specConstants[i]->constant->GetIDName().c_str());
  }

  retDisasm += "\n";

  for(size_t f = 0; f < funcs.size(); f++)
  {
    SPVFunction *func = funcs[f]->func;
    RDCASSERT(func && func->retType && func->funcType);

    std::string args = "";

    for(size_t a = 0; a < func->funcType->children.size(); a++)
    {
      const rdcpair<SPVTypeData *, std::string> &arg = func->funcType->children[a];
      RDCASSERT(a < func->arguments.size());
      const SPVInstruction *argname = func->arguments[a];

      if(argname->str.empty())
        args += arg.first->GetName();
      else
        args += StringFormat::Fmt("%s %s", arg.first->GetName().c_str(), argname->str.c_str());

      if(a + 1 < func->funcType->children.size())
        args += ", ";
    }

    retDisasm += StringFormat::Fmt("%s %s(%s)%s {\n", func->retType->GetName().c_str(),
                                   funcs[f]->str.c_str(), args.c_str(),
                                   OptionalFlagString(func->control).c_str());

    // local copy of variables vector
    std::vector<SPVInstruction *> vars = func->variables;
    std::vector<SPVInstruction *> funcops;

    for(size_t b = 0; b < func->blocks.size(); b++)
    {
      SPVInstruction *block = func->blocks[b];

      // don't push first label in a function
      if(b > 0)
        funcops.push_back(block);    // OpLabel

      std::set<SPVInstruction *> ignore_items;

      for(size_t i = 0; i < block->block->instructions.size(); i++)
      {
        SPVInstruction *instr = block->block->instructions[i];

        if(ignore_items.find(instr) == ignore_items.end())
          funcops.push_back(instr);

        // we can't inline the arguments to an OpPhi
        if(instr->op && instr->opcode != spv::OpPhi)
        {
          int maxcomplex = instr->op->complexity;

          for(size_t a = 0; a < instr->op->arguments.size(); a++)
          {
            SPVInstruction *arg = instr->op->arguments[a];

            if(arg->op)
            {
              // allow less inlining in composite constructs
              int maxAllowedComplexity = NO_INLINE_COMPLEXITY;
              if(instr->opcode == spv::OpCompositeConstruct)
                maxAllowedComplexity = RDCMIN(NO_INLINE_COMPLEXITY - 1, maxAllowedComplexity);

              // don't fold up too complex an operation
              // allow some ops to have multiple arguments, others with many
              // arguments should not be inlined
              if(arg->op->complexity >= maxAllowedComplexity ||
                 (arg->op->arguments.size() > 2 && arg->opcode != spv::OpAccessChain &&
                  arg->opcode != spv::OpArrayLength && arg->opcode != spv::OpInBoundsAccessChain &&
                  arg->opcode != spv::OpSelect && arg->opcode != spv::OpCompositeConstruct))
                continue;

              // for anything but store's dest argument
              if(instr->opcode != spv::OpStore || a > 0)
              {
                // Do not inline this argument if it relies on a load from a
                // variable that is written to between the argument and this
                // op that we're inlining into, as that changes the meaning.
                if(!IsUnmodified(func, arg, instr))
                  continue;
              }

              maxcomplex = RDCMAX(arg->op->complexity, maxcomplex);
            }

            erase_item(funcops, arg);

            instr->op->inlineArgs |= (1 << a);
          }

          instr->op->complexity = maxcomplex;

          if(instr->opcode != spv::OpStore && instr->opcode != spv::OpLoad &&
             instr->opcode != spv::OpCompositeExtract &&
             instr->opcode != spv::OpVectorExtractDynamic && instr->op->inlineArgs)
            instr->op->complexity++;

          // we try to merge away temp variables that are only used for a single store then a single
          // load later. We can only do this if:
          //  - The Load we're looking is the only load in this function of the variable
          //  - The Load is preceeded by precisely one Store - not 0 or 2+
          //  - The previous store is 'pure', ie. does not depend on any mutated variables
          //    so it is safe to re-order to where the Load is.
          //  - The variable in question is a function variable
          //
          // If those conditions are met then we can remove the previous store, inline it as the
          // load
          // function argument (instead of the variable), and remove the variable.

          if(instr->opcode == spv::OpLoad && funcops.size() > 1 && instr->op->arguments[0]->var &&
             instr->op->arguments[0]->var->storage == spv::StorageClassFunction)
          {
            SPVInstruction *prevstore = NULL;
            int storecount = 0;

            for(size_t o = 0; o < funcops.size(); o++)
            {
              SPVInstruction *previnstr = funcops[o];
              if(previnstr->opcode == spv::OpStore &&
                 previnstr->op->arguments[0] == instr->op->arguments[0])
              {
                prevstore = previnstr;
                storecount++;
                if(storecount > 1)
                  break;
              }
            }

            if(storecount == 1 && IsUnmodified(func, prevstore, instr))
            {
              bool otherload = false;

              // note variables have function scope, need to check all blocks in this function
              for(size_t o = 0; o < func->blocks.size(); o++)
              {
                SPVInstruction *otherblock = func->blocks[o];

                for(size_t l = 0; l < otherblock->block->instructions.size(); l++)
                {
                  SPVInstruction *otherinstr = otherblock->block->instructions[l];
                  if(otherinstr != instr && otherinstr->opcode == spv::OpLoad &&
                     otherinstr->op->arguments[0] == instr->op->arguments[0])
                  {
                    otherload = true;
                    break;
                  }
                }
              }

              if(!otherload)
              {
                instr->op->complexity = RDCMAX(instr->op->complexity, prevstore->op->complexity);
                erase_item(vars, instr->op->arguments[0]);
                erase_item(funcops, prevstore);
                instr->op->arguments[0] = prevstore;
              }
            }
          }

          // if we have a store from a temp ID, immediately following the op
          // that produced that temp ID, we can combine these trivially
          if((instr->opcode == spv::OpStore || instr->opcode == spv::OpCompositeInsert) &&
             funcops.size() > 1)
          {
            if(instr->op->arguments[1] == funcops[funcops.size() - 2])
            {
              erase_item(funcops, instr->op->arguments[1]);
              if(instr->op->arguments[1]->op)
                instr->op->complexity =
                    RDCMAX(instr->op->complexity, instr->op->arguments[1]->op->complexity);
              instr->op->inlineArgs |= 2;
            }
          }

          // special handling for function call to inline temporary pointer variables
          // created for passing parameters
          if(instr->opcode == spv::OpFunctionCall)
          {
            for(size_t a = 0; a < instr->op->arguments.size(); a++)
            {
              SPVInstruction *arg = instr->op->arguments[a];

              // if this argument has
              //  - only one usage as a store target before the function call
              //  = then it's an in parameter, and we can fold it in.
              //
              //  - only one usage as a load target after the function call
              //  = then it's an out parameter, we can fold it in as long as
              //    the usage after is in a Store(a) = Load(param) case
              //
              //  - exactly one usage as store before, and load after, such that
              //    it is Store(param) = Load(a) .... Store(a) = Load(param)
              //  = then it's an inout parameter, and we can fold it in

              bool canReplace = true;
              SPVInstruction *storeBefore = NULL;
              SPVInstruction *loadAfter = NULL;
              size_t storeIdx = block->block->instructions.size();
              size_t loadIdx = block->block->instructions.size();

              for(size_t j = 0; j < i; j++)
              {
                SPVInstruction *searchInst = block->block->instructions[j];
                for(size_t aa = 0; searchInst->op && aa < searchInst->op->arguments.size(); aa++)
                {
                  if(searchInst->op->arguments[aa]->id == arg->id)
                  {
                    if(searchInst->opcode == spv::OpStore)
                    {
                      // if it's used in multiple stores, it can't be folded
                      if(storeBefore)
                      {
                        canReplace = false;
                        break;
                      }
                      storeBefore = searchInst;
                      storeIdx = j;
                    }
                    else
                    {
                      // if it's used in anything but a store, it can't be folded
                      canReplace = false;
                      break;
                    }
                  }
                }

                // if it's used in a condition, it can't be folded
                if(searchInst->flow && searchInst->flow->condition &&
                   searchInst->flow->condition->id == arg->id)
                  canReplace = false;

                if(!canReplace)
                  break;
              }

              for(size_t j = i + 1; j < block->block->instructions.size(); j++)
              {
                SPVInstruction *searchInst = block->block->instructions[j];
                for(size_t aa = 0; searchInst->op && aa < searchInst->op->arguments.size(); aa++)
                {
                  if(searchInst->op->arguments[aa]->id == arg->id)
                  {
                    if(searchInst->opcode == spv::OpLoad)
                    {
                      // if it's used in multiple load, it can't be folded
                      if(loadAfter)
                      {
                        canReplace = false;
                        break;
                      }
                      loadAfter = searchInst;
                      loadIdx = j;
                    }
                    else
                    {
                      // if it's used in anything but a load, it can't be folded
                      canReplace = false;
                      break;
                    }
                  }
                }

                // if it's used in a condition, it can't be folded
                if(searchInst->flow && searchInst->flow->condition &&
                   searchInst->flow->condition->id == arg->id)
                  canReplace = false;

                if(!canReplace)
                  break;
              }

              if(canReplace)
              {
                // in parameter
                if(storeBefore && !loadAfter)
                {
                  erase_item(funcops, storeBefore);

                  erase_item(vars, instr->op->arguments[a]);

                  // pass function parameter directly from where the store was coming from
                  instr->op->arguments[a] = storeBefore->op->arguments[1];
                }

                // out or inout parameter
                if(loadAfter)
                {
                  // need to check the load afterwards is only ever used in a store operation

                  SPVInstruction *storeUse = NULL;

                  for(size_t j = loadIdx + 1; j < block->block->instructions.size(); j++)
                  {
                    SPVInstruction *searchInst = block->block->instructions[j];

                    for(size_t aa = 0; searchInst->op && aa < searchInst->op->arguments.size(); aa++)
                    {
                      if(searchInst->op->arguments[aa] == loadAfter)
                      {
                        if(searchInst->opcode == spv::OpStore)
                        {
                          // if it's used in multiple stores, it can't be folded
                          if(storeUse)
                          {
                            canReplace = false;
                            break;
                          }
                          storeUse = searchInst;
                        }
                        else
                        {
                          // if it's used in anything but a store, it can't be folded
                          canReplace = false;
                          break;
                        }
                      }
                    }

                    // if it's used in a condition, it can't be folded
                    if(searchInst->flow && searchInst->flow->condition == loadAfter)
                      canReplace = false;

                    if(!canReplace)
                      break;
                  }

                  if(canReplace && storeBefore != NULL)
                  {
                    // for the inout parameter case, we also need to verify that
                    // the Store() before the function call comes from a Load(),
                    // and that the variable being Load()'d is identical to the
                    // variable in the Store() in storeUse that we've found

                    if(storeBefore->op->arguments[1]->opcode == spv::OpLoad &&
                       storeBefore->op->arguments[1]->op->arguments[0]->id ==
                           storeUse->op->arguments[0]->id)
                    {
                      erase_item(funcops, storeBefore);
                    }
                    else
                    {
                      canReplace = false;
                    }
                  }

                  if(canReplace)
                  {
                    // we haven't reached this store instruction yet, so need to mark that
                    // it has been folded and should be skipped
                    ignore_items.insert(storeUse);

                    erase_item(vars, instr->op->arguments[a]);

                    // pass argument directly
                    instr->op->arguments[a] = storeUse->op->arguments[0];
                  }
                }
              }
            }
          }
        }
      }

      if(block->block->mergeFlow)
        funcops.push_back(block->block->mergeFlow);
      if(block->block->exitFlow)
      {
        // branch conditions are inlined unless otherwise required
        SPVInstruction *cond = block->block->exitFlow->flow->condition;
        if(cond && cond->op && cond->op->complexity < NEVER_INLINE_COMPLEXITY)
          erase_item(funcops, cond);

        // return values are inlined
        if(block->block->exitFlow->opcode == spv::OpReturnValue)
        {
          SPVInstruction *arg = ids[block->block->exitFlow->flow->targets[0]];

          erase_item(funcops, arg);
        }

        funcops.push_back(block->block->exitFlow);
      }
    }

    // keep track of switch statements, as they can contain
    //     Branch 123
    //     Label 123
    // that we want to keep, to identify breaks and fallthroughs
    std::vector<rdcpair<uint32_t, SPVFlowControl *> > switchstack;

    // find redundant branch/label pairs
    for(size_t l = 0; l < funcops.size() - 1;)
    {
      if(funcops[l]->opcode == spv::OpSwitch)
      {
        RDCASSERT(l > 0 && funcops[l - 1]->opcode == spv::OpSelectionMerge);
        switchstack.push_back(make_rdcpair(funcops[l - 1]->flow->targets[0], funcops[l]->flow));
      }

      if(funcops[l]->opcode == spv::OpLabel)
      {
        if(!switchstack.empty() && switchstack.back().first == funcops[l]->id)
          switchstack.pop_back();
      }

      if(funcops[l]->opcode == spv::OpBranch)
      {
        uint32_t branchTarget = funcops[l]->flow->targets[0];

        bool skip = false;

        for(size_t sw = 0; sw < switchstack.size(); sw++)
        {
          if(switchstack[sw].first == branchTarget)
          {
            l++;
            skip = true;
            break;
          }

          for(size_t t = 0; t < switchstack[sw].second->targets.size(); t++)
          {
            if(switchstack[sw].second->targets[t] == branchTarget)
            {
              l++;
              skip = true;
              break;
            }
          }
        }

        if(skip)
          continue;

        if(funcops[l + 1]->opcode == spv::OpLabel && branchTarget == funcops[l + 1]->id)
        {
          uint32_t label = funcops[l + 1]->id;

          bool refd = false;

          // see if this label is a target anywhere else
          for(size_t b = 0; b < funcops.size(); b++)
          {
            if(l == b)
              continue;

            if(funcops[b]->flow)
            {
              for(size_t t = 0; t < funcops[b]->flow->targets.size(); t++)
              {
                if(funcops[b]->flow->targets[t] == label)
                {
                  refd = true;
                  break;
                }
              }

              if(refd)
                break;
            }
          }

          if(!refd)
          {
            funcops.erase(funcops.begin() + l);
            funcops.erase(funcops.begin() + l);
            continue;
          }
          else
          {
            // if it is refd, we can at least remove the goto
            funcops.erase(funcops.begin() + l);
            continue;
          }
        }
      }

      l++;
    }

    // if we have a vector compositeextract that is only ever used in a
    // subsequent compositeconstruct which will just be inlined directly src-to-dest
    // then remove the extract. This assumes though there will be no other uses of
    // the extract elsewhere
    for(size_t o = 0; o < funcops.size();)
    {
      if(funcops[o]->opcode == spv::OpCompositeExtract && funcops[o]->op->arguments[0]->op &&
         funcops[o]->op->arguments[0]->op->type->type == SPVTypeData::eVector)
      {
        // count how many times this extract is used in constructing a vector
        uint32_t constructUses = 0;

        for(size_t p = o + 1; p < funcops.size(); p++)
        {
          SPVInstruction *useInstr = NULL;

          // return value is special because it doesn't hold a SPVInstruction* to its
          // return value, so we check it manually
          if(funcops[p]->opcode == spv::OpReturnValue)
          {
            if(funcops[o]->id == funcops[p]->flow->targets[0])
              useInstr = funcops[p];
            else
            {
              SPVInstruction *instr = ids[funcops[p]->flow->targets[0]];

              if(instr && instr->op)
                FindFirstInstructionUse(instr, funcops[o], &useInstr);
            }
          }

          // find out if this instruction uses the extract somewhere
          if(useInstr == NULL)
          {
            if(!funcops[p]->op)
              continue;

            FindFirstInstructionUse(funcops[p], funcops[o], &useInstr);
          }

          if(useInstr == NULL)
            continue;

          if(useInstr->opcode != spv::OpCompositeConstruct ||
             useInstr->op->type->type != SPVTypeData::eVector)
          {
            // extract is used in a non-construct, or not constructing a vector (e.g. a struct)
            // so pretend the extract is used multiple times so that it can't be removed
            constructUses = 10;
            break;
          }
          else
          {
            // it was used in a construct of a vector, increment
            constructUses++;

            // if it's been used more than once, break
            if(constructUses > 1)
              break;
          }
        }

        // if it's only been used once, then we can safely remove the extract
        // as it will be in-lined at disassembly time. Otherwise just continue
        if(constructUses == 1)
          funcops.erase(funcops.begin() + o);
        else
          o++;

        continue;
      }

      o++;
    }

    RDCASSERT(switchstack.empty());

    size_t tabSize = 2;
    size_t indent = tabSize;

    bool *varDeclared = new bool[vars.size()];
    for(size_t v = 0; v < vars.size(); v++)
      varDeclared[v] = false;

// if we're declaring variables at the top of the function rather than at first use
#if C_VARIABLE_DECLARATIONS
    for(size_t v = 0; v < vars.size(); v++)
    {
      RDCASSERT(vars[v]->var && vars[v]->var->type);
      retDisasm += std::string(indent, ' ') +
                   vars[v]->var->type->DeclareVariable(vars[v]->decorations, vars[v]->GetIDName()) +
                   ";\n";

      varDeclared[v] = true;
    }

    if(!vars.empty())
      retDisasm += "\n";
#endif

    struct sel
    {
      sel(uint32_t i) : id(i), elseif(false) {}
      uint32_t id;
      bool elseif;
    };

    std::vector<sel> selectionstack;
    std::vector<uint32_t> elsestack;

    std::vector<uint32_t> loopheadstack;
    std::vector<uint32_t> loopstartstack;
    std::vector<uint32_t> loopmergestack;

    std::string funcDisassembly = "";

    for(size_t o = 0; o < funcops.size(); o++)
    {
      if(funcops[o]->opcode == spv::OpLabel)
      {
        bool handled = false;

        if(!switchstack.empty())
        {
          if(switchstack.back().first == funcops[o]->id)
          {
            // handle the end of the switch block
            indent -= tabSize;

            handled = true;

            funcDisassembly += std::string(indent, ' ');
            funcDisassembly += "}\n";
            selectionstack.pop_back();
            switchstack.pop_back();
          }
          else
          {
            SPVInstruction *cond = switchstack.back().second->condition;
            std::vector<uint32_t> &targets = switchstack.back().second->targets;
            std::vector<uint32_t> &values = switchstack.back().second->literals;
            for(size_t t = 0; t < targets.size(); t++)
            {
              if(targets[t] == funcops[o]->id)
              {
                handled = true;

                if(t == targets.size() - 1)
                {
                  funcDisassembly += std::string(indent - tabSize, ' ');
                  funcDisassembly += "default:\n";
                }
                else
                {
                  RDCASSERT(t < values.size());
                  funcDisassembly += std::string(indent - tabSize, ' ');

                  if(cond->op && cond->op->type->type == SPVTypeData::eSInt)
                  {
                    funcDisassembly += StringFormat::Fmt("case %d:\n", values[t]);
                  }
                  else
                  {
                    funcDisassembly += StringFormat::Fmt("case %u:\n", values[t]);
                  }
                }
              }
            }
          }
        }

        if(handled)
        {
        }
        else if(!elsestack.empty() && elsestack.back() == funcops[o]->id)
        {
          // handle meeting an else block
          funcDisassembly += std::string(indent - tabSize, ' ');
          funcDisassembly += "} else ";

          if(o + 2 < funcops.size() && funcops[o + 1]->opcode == spv::OpSelectionMerge &&
             funcops[o + 2]->opcode == spv::OpBranchConditional)
          {
            // handle else if, remove the indent now as the else if will be on the same level
            indent -= tabSize;
            selectionstack.back().elseif = true;
          }
          else
          {
            funcDisassembly += "{\n";
          }
          elsestack.pop_back();
        }
        else if(!selectionstack.empty() && selectionstack.back().id == funcops[o]->id)
        {
          // handle meeting a selection merge block

          // if we have hit an else if, the indent has already been
          // removed
          if(!selectionstack.back().elseif)
          {
            indent -= tabSize;
            funcDisassembly += std::string(indent, ' ');
            funcDisassembly += "}\n";
          }
          selectionstack.pop_back();
        }
        else if(!loopmergestack.empty() && loopmergestack.back() == funcops[o]->id)
        {
          // handle meeting a loop merge block
          indent -= tabSize;

          funcDisassembly += std::string(indent, ' ');
          funcDisassembly += "}\n";

          loopheadstack.pop_back();
          loopstartstack.pop_back();
          loopmergestack.pop_back();
        }
        else if(!loopstartstack.empty() && loopstartstack.back() == funcops[o]->id)
        {
          // completely skip a label at the start of the loop. It's implicit from braces
        }
        else if(funcops[o]->block->mergeFlow &&
                funcops[o]->block->mergeFlow->opcode == spv::OpLoopMerge)
        {
          loopheadstack.push_back(funcops[o]->id);
          loopstartstack.push_back(funcops[o]->block->exitFlow->flow->targets[0]);
          loopmergestack.push_back(funcops[o]->block->mergeFlow->flow->targets[0]);

          // should be either unconditional, or false from the condition should jump straight to
          // merge block
          RDCASSERT(funcops[o]->block->exitFlow->flow->targets.size() == 1 ||
                    funcops[o]->block->exitFlow->flow->targets[1] ==
                        funcops[o]->block->mergeFlow->flow->targets[0]);

          // this block is a loop header
          // TODO handle if the loop header condition expression isn't sufficiently in-lined.
          // We need to force inline it.
          funcDisassembly += std::string(indent, ' ');
          if(funcops[o]->block->exitFlow->flow->condition)
          {
            funcDisassembly +=
                "while(" + funcops[o]->block->exitFlow->flow->condition->Disassemble(ids, true) +
                ") {\n";
          }
          else
          {
            bool foundCondition = false;

            // check to see if we have a loopmerge and branchconditional right after this block
            if(o + 3 < funcops.size() && funcops[o]->block->mergeFlow == funcops[o + 1] &&
               funcops[o + 2]->opcode == spv::OpBranchConditional &&
               funcops[o + 3]->opcode == spv::OpLabel)
            {
              uint32_t nextLabel = funcops[o + 3]->id;

              // check if this branch conditional is jumping to a label immediately after or
              // the exit point. The condition could be reversed to check either direction
              if(funcops[o + 2]->flow->targets[0] == nextLabel &&
                 funcops[o + 2]->flow->targets[1] == funcops[o]->block->mergeFlow->flow->targets[0])
              {
                funcDisassembly += "while(" + funcops[o + 2]->Disassemble(ids, true) + ") {\n";

                // skip all of the above that we just used up
                o += 3;
                foundCondition = true;
              }
              else if(funcops[o + 2]->flow->targets[1] == nextLabel &&
                      funcops[o + 2]->flow->targets[0] ==
                          funcops[o]->block->mergeFlow->flow->targets[0])
              {
                funcDisassembly += "while(!(" + funcops[o + 2]->Disassemble(ids, true) + ")) {\n";

                // skip all of the above that we just used up
                o += 3;
                foundCondition = true;
              }
            }

            if(!foundCondition)
              funcDisassembly += "while(true) {\n";
          }

          indent += tabSize;
        }
        else
        {
          funcDisassembly += funcops[o]->Disassemble(ids, false) + "\n";
        }
      }
      else if(funcops[o]->opcode == spv::OpBranch)
      {
        bool handled = false;

        if(!switchstack.empty())
        {
          if(switchstack.back().first == funcops[o]->flow->targets[0])
          {
            // this branch is to the selection merge label of the switch statement, it must
            // be a break instruction
            funcDisassembly += std::string(indent, ' ');
            funcDisassembly += "break;\n";

            handled = true;
          }
          else
          {
            std::vector<uint32_t> &targets = switchstack.back().second->targets;
            for(size_t t = 0; t < targets.size(); t++)
            {
              if(targets[t] == funcops[o]->flow->targets[0])
              {
                // if we're branching to one of the targets of the switch statement,
                // assume this is fall-through. Normally only the switch itself would
                // branch to one of these labels, but if a case branches to another
                // that is a representation of fall-through.
                // Note in this case the label will also be the next funcop, but this
                // is required by the spec so we just assert
                RDCASSERT(o + 1 < funcops.size() && funcops[o + 1]->id == targets[t]);
                handled = true;
              }
            }
          }
        }

        if(handled)
        {
        }
        else if(!selectionstack.empty() && funcops[o]->flow->targets[0] == selectionstack.back().id)
        {
          // if we're at the end of a true if path there will be a goto to
          // the merge block before the false path label. Don't output it
        }
        else if(!loopheadstack.empty() && funcops[o]->flow->targets[0] == loopheadstack.back())
        {
          if(o + 1 < funcops.size() && funcops[o + 1]->opcode == spv::OpLabel &&
             funcops[o + 1]->id == loopmergestack.back())
          {
            // skip any gotos at the end of a loop jumping back to the header
            // block to do another loop
          }
          else
          {
            // if we're skipping to the header of the loop before the end, this is a continue
            funcDisassembly += std::string(indent, ' ');
            funcDisassembly += "continue;\n";
          }
        }
        else if(!loopmergestack.empty() && funcops[o]->flow->targets[0] == loopmergestack.back())
        {
          // if we're skipping to the merge of the loop without going through the
          // branch conditional, this is a break
          funcDisassembly += std::string(indent, ' ');
          funcDisassembly += "break;\n";
        }
        else
        {
          funcDisassembly += std::string(indent, ' ');
          funcDisassembly += funcops[o]->Disassemble(ids, false) + ";\n";
        }
      }
      else if(funcops[o]->opcode == spv::OpLoopMerge)
      {
        // handled above when this block started
        o++;    // skip the branch conditional op
      }
      else if(funcops[o]->opcode == spv::OpSelectionMerge)
      {
        RDCASSERT(o + 1 < funcops.size());

        bool elseif = false;
        if(!selectionstack.empty())
          elseif = selectionstack.back().elseif;

        selectionstack.push_back(sel(funcops[o]->flow->targets[0]));

        o++;

        if(funcops[o]->opcode == spv::OpBranchConditional)
        {
          if(!elseif)
            funcDisassembly += std::string(indent, ' ');
          funcDisassembly += "if(" + funcops[o]->Disassemble(ids, false) + ") {\n";

          indent += tabSize;

          // does the branch have an else case
          if(funcops[o]->flow->targets[1] != selectionstack.back().id)
            elsestack.push_back(funcops[o]->flow->targets[1]);

          RDCASSERT(o + 1 < funcops.size() && funcops[o + 1]->opcode == spv::OpLabel &&
                    funcops[o + 1]->id == funcops[o]->flow->targets[0]);
          o++;    // skip outputting this label, it becomes our { essentially
        }
        else if(funcops[o]->opcode == spv::OpSwitch)
        {
          funcDisassembly += std::string(indent, ' ');
          funcDisassembly += funcops[o]->Disassemble(ids, false) + " {\n";

          indent += tabSize;

          switchstack.push_back(make_rdcpair(selectionstack.back().id, funcops[o]->flow));
        }
        else
        {
          RDCERR("Unexpected opcode following selection merge");
        }
      }
      else if(funcops[o]->opcode == spv::OpCompositeInsert && o + 1 < funcops.size() &&
              funcops[o + 1]->opcode == spv::OpStore)
      {
        // try to merge this load-hit-store construct:
        // {id} = CompositeInsert <somevar> <foo> indices...
        // Store <somevar> {id}

        uint32_t loadID = 0;

        if(funcops[o]->op->arguments[0]->opcode == spv::OpLoad)
          loadID = funcops[o]->op->arguments[0]->op->arguments[0]->id;

        if(loadID == funcops[o + 1]->op->arguments[0]->id)
        {
          // merge
          SPVInstruction *loadhit = funcops[o];
          SPVInstruction *store = funcops[o + 1];

          o++;

          bool printed = false;

          SPVInstruction *storeVar = store->op->arguments[0];

// declare variables at first use
#if !C_VARIABLE_DECLARATIONS
          for(size_t v = 0; v < vars.size(); v++)
          {
            if(!varDeclared[v] && vars[v] == storeVar)
            {
              // if we're in a scope, be conservative as the variable might be
              // used after the scope - print the declaration before the scope
              // begins and continue as normal.
              if(indent > tabSize)
              {
                retDisasm += std::string(tabSize, ' ');
                retDisasm +=
                    vars[v]->var->type->DeclareVariable(vars[v]->decorations, vars[v]->GetIDName()) +
                    ";\n";
              }
              else
              {
                funcDisassembly += std::string(indent, ' ');
                funcDisassembly +=
                    vars[v]->var->type->DeclareVariable(vars[v]->decorations, vars[v]->GetIDName());

                printed = true;
              }

              varDeclared[v] = true;
            }
          }
#endif

          if(!printed)
          {
            std::string storearg;
            store->op->GetArg(ids, 0, storearg);

            funcDisassembly += std::string(indent, ' ');
            funcDisassembly += storearg;
          }
          funcDisassembly +=
              loadhit->Disassemble(ids, true);    // inline compositeinsert includes ' = '
          funcDisassembly += ";\n";

          loadhit->line = (int)o;
        }
        else
        {
          // print separately
          funcDisassembly += std::string(indent, ' ');
          funcDisassembly += funcops[o]->Disassemble(ids, false) + ";\n";
          funcops[o]->line = (int)o;

          o++;

          SPVInstruction *storeVar = funcops[o]->op->arguments[0];

          bool printed = false;

// declare variables at first use
#if !C_VARIABLE_DECLARATIONS
          for(size_t v = 0; v < vars.size(); v++)
          {
            if(!varDeclared[v] && vars[v] == storeVar)
            {
              // if we're in a scope, be conservative as the variable might be
              // used after the scope - print the declaration before the scope
              // begins and continue as normal.
              if(indent > tabSize)
              {
                retDisasm += std::string(tabSize, ' ');
                retDisasm +=
                    vars[v]->var->type->DeclareVariable(vars[v]->decorations, vars[v]->GetIDName()) +
                    ";\n";
              }
              else
              {
                funcDisassembly += std::string(indent, ' ');
                funcDisassembly +=
                    vars[v]->var->type->DeclareVariable(vars[v]->decorations, vars[v]->GetIDName()) +
                    " = ";
                funcDisassembly += funcops[o]->Disassemble(ids, true) + ";\n";

                printed = true;
              }

              varDeclared[v] = true;
            }
          }
#endif

          if(!printed)
          {
            funcDisassembly += std::string(indent, ' ');
            funcDisassembly += funcops[o]->Disassemble(ids, false) + ";\n";
          }
        }
      }
      else if(funcops[o]->opcode == spv::OpReturn && o == funcops.size() - 1)
      {
        // don't print the return statement if it's the last statement in a function
        break;
      }
      else if(funcops[o]->opcode == spv::OpStore)
      {
        SPVInstruction *storeVar = funcops[o]->op->arguments[0];

        bool printed = false;

// declare variables at first use
#if !C_VARIABLE_DECLARATIONS
        for(size_t v = 0; v < vars.size(); v++)
        {
          if(!varDeclared[v] && vars[v] == storeVar)
          {
            // if we're in a scope, be conservative as the variable might be
            // used after the scope - print the declaration before the scope
            // begins and continue as normal.
            if(indent > tabSize)
            {
              retDisasm += std::string(tabSize, ' ');
              retDisasm +=
                  vars[v]->var->type->DeclareVariable(vars[v]->decorations, vars[v]->GetIDName()) +
                  ";\n";
            }
            else
            {
              funcDisassembly += std::string(indent, ' ');
              funcDisassembly +=
                  vars[v]->var->type->DeclareVariable(vars[v]->decorations, vars[v]->GetIDName()) +
                  " = ";
              funcDisassembly += funcops[o]->Disassemble(ids, true) + ";\n";

              printed = true;
            }

            varDeclared[v] = true;
          }
        }
#endif

        if(!printed)
        {
          funcDisassembly += std::string(indent, ' ');
          funcDisassembly += funcops[o]->Disassemble(ids, false) + ";\n";
        }
      }
      else
      {
        funcDisassembly += std::string(indent, ' ');
        funcDisassembly += funcops[o]->Disassemble(ids, false) + ";\n";
      }

      funcops[o]->line = (int)o;
    }

    RDCASSERT(switchstack.empty());
    RDCASSERT(selectionstack.empty());
    RDCASSERT(elsestack.empty());
    RDCASSERT(loopheadstack.empty());
    RDCASSERT(loopstartstack.empty());
    RDCASSERT(loopmergestack.empty());

// declare any variables that didn't get declared inline somewhere above
#if !C_VARIABLE_DECLARATIONS
    for(size_t v = 0; v < vars.size(); v++)
    {
      if(varDeclared[v])
        continue;

      RDCASSERT(vars[v]->var && vars[v]->var->type);
      retDisasm += std::string(indent, ' ') +
                   vars[v]->var->type->DeclareVariable(vars[v]->decorations, vars[v]->GetIDName()) +
                   ";\n";
    }

    if(!vars.empty())
      retDisasm += "\n";
#endif

    retDisasm += funcDisassembly;

    SAFE_DELETE_ARRAY(varDeclared);

    retDisasm += StringFormat::Fmt("} // %s\n\n", funcs[f]->str.c_str());
  }

  return retDisasm;
}

void MakeConstantBlockVariables(SPVTypeData *structType, uint32_t arraySize,
                                uint32_t arrayByteStride, rdcarray<ShaderConstant> &cblock);

void MakeConstantBlockVariable(ShaderConstant &outConst, SPVTypeData *type, const std::string &name,
                               const std::vector<SPVDecoration> &decorations)
{
  outConst.name = name;

  outConst.defaultValue = 0;

  for(size_t d = 0; d < decorations.size(); d++)
  {
    if(decorations[d].decoration == spv::DecorationOffset)
    {
      outConst.byteOffset = decorations[d].val;
      break;
    }
  }

  outConst.type.descriptor.elements = 1;
  outConst.type.descriptor.arrayByteStride = 0;
  outConst.type.descriptor.matrixByteStride = 0;

  if(type->type == SPVTypeData::eArray)
  {
    if(type->arraySize == ~0U)
    {
      outConst.type.descriptor.elements = 1;    // TODO need to handle 'array of undefined size'
    }
    else
    {
      outConst.type.descriptor.elements = type->arraySize;
    }

    bool foundArrayStride = false, foundMatrixStride = false;

    for(size_t d = 0; d < decorations.size(); d++)
    {
      if(decorations[d].decoration == spv::DecorationArrayStride)
      {
        outConst.type.descriptor.arrayByteStride = decorations[d].val;
        foundArrayStride = true;
      }
      if(decorations[d].decoration == spv::DecorationMatrixStride)
      {
        outConst.type.descriptor.matrixByteStride = (uint8_t)decorations[d].val;
        foundMatrixStride = true;
      }

      if(foundMatrixStride && foundArrayStride)
        break;
    }

    for(size_t d = 0; !foundArrayStride && type->decorations && d < type->decorations->size(); d++)
    {
      if((*type->decorations)[d].decoration == spv::DecorationArrayStride)
      {
        outConst.type.descriptor.arrayByteStride = (*type->decorations)[d].val;
        break;
      }
    }

    for(size_t d = 0; !foundMatrixStride && type->decorations && d < type->decorations->size(); d++)
    {
      if((*type->decorations)[d].decoration == spv::DecorationMatrixStride)
      {
        outConst.type.descriptor.matrixByteStride = (uint8_t)(*type->decorations)[d].val;
        break;
      }
    }

    type = type->baseType;
  }

  if(type->type == SPVTypeData::eVector || type->type == SPVTypeData::eMatrix)
  {
    outConst.type.descriptor.type = type->baseType->GetVarType();

    outConst.type.descriptor.rowMajorStorage = (type->type == SPVTypeData::eVector);

    for(size_t d = 0; d < decorations.size(); d++)
    {
      if(decorations[d].decoration == spv::DecorationRowMajor)
        outConst.type.descriptor.rowMajorStorage = true;
      if(decorations[d].decoration == spv::DecorationMatrixStride)
        outConst.type.descriptor.matrixByteStride = (uint8_t)decorations[d].val;
    }

    if(type->type == SPVTypeData::eMatrix)
    {
      outConst.type.descriptor.rows = (uint8_t)type->vectorSize;
      outConst.type.descriptor.columns = (uint8_t)type->matrixSize;
    }
    else
    {
      outConst.type.descriptor.rows = 1;
      outConst.type.descriptor.columns = (uint8_t)type->vectorSize;
    }

    outConst.type.descriptor.name = type->GetName();
  }
  else if(type->IsScalar())
  {
    outConst.type.descriptor.type = type->GetVarType();
    outConst.type.descriptor.rowMajorStorage = true;
    outConst.type.descriptor.rows = 1;
    outConst.type.descriptor.columns = 1;

    outConst.type.descriptor.name = type->GetName();
  }
  else
  {
    outConst.type.descriptor.type = VarType::Float;
    outConst.type.descriptor.rowMajorStorage = false;
    outConst.type.descriptor.rows = 0;
    outConst.type.descriptor.columns = 0;

    outConst.type.descriptor.name = type->GetName();

    MakeConstantBlockVariables(type, outConst.type.descriptor.elements,
                               outConst.type.descriptor.arrayByteStride, outConst.type.members);

    if(type->type == SPVTypeData::eArray)
    {
      // if the inner type is an array, it will be expanded in our members list. So don't also
      // redundantly keep the element count
      outConst.type.descriptor.elements = 1;
    }
  }
}

void MakeConstantBlockVariables(SPVTypeData *structType, uint32_t arraySize,
                                uint32_t arrayByteStride, rdcarray<ShaderConstant> &cblock)
{
  // we get here for multi-dimensional arrays
  if(structType->type == SPVTypeData::eArray)
  {
    std::vector<SPVDecoration> empty;

    uint32_t relativeOffset = 0;

    cblock.resize(arraySize);
    for(uint32_t i = 0; i < arraySize; i++)
    {
      MakeConstantBlockVariable(cblock[i], structType, StringFormat::Fmt("[%u]", i),
                                structType->decorations ? *structType->decorations : empty);

      cblock[i].byteOffset = relativeOffset;

      relativeOffset += arrayByteStride;
    }

    return;
  }

  if(structType->children.empty())
    return;

  cblock.resize(structType->children.size());
  for(size_t i = 0; i < structType->children.size(); i++)
    MakeConstantBlockVariable(cblock[i], structType->children[i].first,
                              structType->children[i].second, structType->childDecorations[i]);
}

uint32_t CalculateMinimumByteSize(const rdcarray<ShaderConstant> &variables)
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

ShaderBuiltin BuiltInToSystemAttribute(ShaderStage stage, const spv::BuiltIn el)
{
  // not complete, might need to expand system attribute list

  switch(el)
  {
    case spv::BuiltInPosition: return ShaderBuiltin::Position;
    case spv::BuiltInPointSize: return ShaderBuiltin::PointSize;
    case spv::BuiltInClipDistance: return ShaderBuiltin::ClipDistance;
    case spv::BuiltInCullDistance: return ShaderBuiltin::CullDistance;
    case spv::BuiltInVertexId: return ShaderBuiltin::VertexIndex;
    case spv::BuiltInInstanceId: return ShaderBuiltin::InstanceIndex;
    case spv::BuiltInPrimitiveId: return ShaderBuiltin::PrimitiveIndex;
    case spv::BuiltInInvocationId:
    {
      if(stage == ShaderStage::Geometry)
        return ShaderBuiltin::GSInstanceIndex;
      else
        return ShaderBuiltin::OutputControlPointIndex;
    }
    case spv::BuiltInLayer: return ShaderBuiltin::RTIndex;
    case spv::BuiltInViewportIndex: return ShaderBuiltin::ViewportIndex;
    case spv::BuiltInTessLevelOuter: return ShaderBuiltin::OuterTessFactor;
    case spv::BuiltInTessLevelInner: return ShaderBuiltin::InsideTessFactor;
    case spv::BuiltInPatchVertices: return ShaderBuiltin::PatchNumVertices;
    case spv::BuiltInFrontFacing: return ShaderBuiltin::IsFrontFace;
    case spv::BuiltInSampleId: return ShaderBuiltin::MSAASampleIndex;
    case spv::BuiltInSamplePosition: return ShaderBuiltin::MSAASamplePosition;
    case spv::BuiltInSampleMask: return ShaderBuiltin::MSAACoverage;
    case spv::BuiltInFragDepth: return ShaderBuiltin::DepthOutput;
    case spv::BuiltInVertexIndex: return ShaderBuiltin::VertexIndex;
    case spv::BuiltInInstanceIndex: return ShaderBuiltin::InstanceIndex;
    case spv::BuiltInBaseVertex: return ShaderBuiltin::BaseVertex;
    case spv::BuiltInBaseInstance: return ShaderBuiltin::BaseInstance;
    case spv::BuiltInDrawIndex: return ShaderBuiltin::DrawIndex;
    case spv::BuiltInViewIndex: return ShaderBuiltin::ViewportIndex;
    case spv::BuiltInFragStencilRefEXT: return ShaderBuiltin::StencilReference;
    default: break;
  }

  return ShaderBuiltin::Undefined;
}

template <typename T>
struct bindpair
{
  Bindpoint map;
  T bindres;

  bindpair(const Bindpoint &m, const T &res) : map(m), bindres(res) {}
  bool operator<(const bindpair &o) const
  {
    if(map.bindset != o.map.bindset)
      return map.bindset < o.map.bindset;

    // sort INVALID_BIND to the end
    if(map.bind == INVALID_BIND && o.map.bind == INVALID_BIND)    // equal
      return false;
    if(map.bind == INVALID_BIND)    // INVALID_BIND not less than anything
      return false;
    if(o.map.bind == INVALID_BIND)    // anything less than INVALID_BIND
      return true;

    return map.bind < o.map.bind;
  }
};

typedef bindpair<ConstantBlock> cblockpair;
typedef bindpair<ShaderResource> shaderrespair;

void AddSignatureParameter(bool isInput, ShaderStage stage, uint32_t id, uint32_t structID,
                           uint32_t &regIndex, std::vector<uint32_t> accessChain, std::string varName,
                           SPVTypeData *type, const std::vector<SPVDecoration> &decorations,
                           std::vector<SigParameter> &sigarray, SPIRVPatchData &patchData)
{
  SigParameter sig;

  sig.needSemanticIndex = false;

  SPIRVPatchData::InterfaceAccess patch;
  patch.accessChain = accessChain;
  patch.ID = id;
  patch.structID = structID;

  bool rowmajor = true;

  sig.regIndex = regIndex;
  for(size_t d = 0; d < decorations.size(); d++)
  {
    if(decorations[d].decoration == spv::DecorationLocation)
      sig.regIndex = decorations[d].val;
    else if(decorations[d].decoration == spv::DecorationBuiltIn)
      sig.systemValue = BuiltInToSystemAttribute(stage, (spv::BuiltIn)decorations[d].val);
    else if(decorations[d].decoration == spv::DecorationRowMajor)
      rowmajor = true;
    else if(decorations[d].decoration == spv::DecorationColMajor)
      rowmajor = false;
  }

  // fragment shader outputs are implicitly colour outputs
  if(stage == ShaderStage::Fragment && type->storage == spv::StorageClassOutput)
    sig.systemValue = ShaderBuiltin::ColorOutput;

  if(type->type == SPVTypeData::ePointer)
    type = type->baseType;

  bool isArray = false;
  uint32_t arraySize = 1;
  if(type->type == SPVTypeData::eArray)
  {
    arraySize = type->arraySize;
    isArray = true;
    type = type->baseType;

    // step through multi-dimensional arrays
    while(type->type == SPVTypeData::eArray)
      type = type->baseType;
  }

  if(type->type == SPVTypeData::eStruct)
  {
    // it's invalid to include built-in and 'normal' outputs in the same struct. One
    // way this can happen is if a SPIR-V generator incorrectly puts in legacy elements
    // into an implicit gl_PerVertex struct, but they don't have a builtin to associate
    // with. We can safely skip these parameters

    // check to see if this struct contains some builtins
    bool hasBuiltins = false;
    for(size_t c = 0; c < type->childDecorations.size(); c++)
    {
      for(size_t d = 0; d < type->childDecorations[c].size(); d++)
      {
        if(type->childDecorations[c][d].decoration == spv::DecorationBuiltIn)
        {
          hasBuiltins = true;
          break;
        }
      }
    }

    for(uint32_t a = 0; a < arraySize; a++)
    {
      patch.accessChain.push_back(0U);

      for(size_t c = 0; c < type->children.size(); c++)
      {
        // if this struct has builtins, see if this child is a builtin
        if(hasBuiltins)
        {
          bool isBuiltin = false;

          for(size_t d = 0; d < type->childDecorations[c].size(); d++)
          {
            if(type->childDecorations[c][d].decoration == spv::DecorationBuiltIn)
            {
              isBuiltin = true;
              break;
            }
          }

          // if it's not a builtin, then skip it
          if(!isBuiltin)
            continue;
        }

        std::string baseName = isArray ? StringFormat::Fmt("%s[%u]", varName.c_str(), a) : varName;

        AddSignatureParameter(isInput, stage, id, type->id, regIndex, patch.accessChain,
                              baseName + "." + type->children[c].second, type->children[c].first,
                              type->childDecorations[c], sigarray, patchData);

        patch.accessChain.back()++;
      }
    }

    return;
  }

  SPVTypeData *primType = type->baseType ? type->baseType : type;

  switch(primType->type)
  {
    case SPVTypeData::eBool:
    case SPVTypeData::eUInt: sig.compType = CompType::UInt; break;
    case SPVTypeData::eSInt: sig.compType = CompType::SInt; break;
    case SPVTypeData::eFloat:
      sig.compType = primType->bitCount == 64 ? CompType::Double : CompType::Float;
      break;
    default: RDCERR("Unexpected base type of input/output signature %u", primType->type); break;
  }

  sig.compCount = type->vectorSize;
  sig.stream = 0;

  sig.regChannelMask = sig.channelUsedMask = (1 << type->vectorSize) - 1;

  // arrays will need an extra access chain index
  if(isArray)
    patch.accessChain.push_back(0U);

  for(uint32_t a = 0; a < arraySize; a++)
  {
    std::string n = varName;

    if(isArray)
    {
      n = StringFormat::Fmt("%s[%u]", varName.c_str(), a);
      sig.arrayIndex = a;
    }

    sig.varName = n;

    if(type->matrixSize == 1)
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
      patch.isMatrix = true;

      for(uint32_t m = 0; m < type->matrixSize; m++)
      {
        SigParameter s = sig;
        s.varName = StringFormat::Fmt("%s:%s%u", n.c_str(), rowmajor ? "row" : "col", m);
        s.regIndex += m;

        RDCASSERT(s.regIndex < 16);

        sigarray.push_back(s);

        if(isInput)
          patchData.inputs.push_back(patch);
        else
          patchData.outputs.push_back(patch);

        regIndex++;

        patch.accessChain.back()++;
        patch.isArraySubsequentElement = true;
      }

      patch.isMatrix = false;
      patch.accessChain.pop_back();
    }

    sig.regIndex += RDCMAX(1U, type->matrixSize);
    if(isArray)
    {
      patch.accessChain.back()++;
      patch.isArraySubsequentElement = true;
    }
  }
}

std::vector<std::string> SPVModule::EntryPoints() const
{
  std::vector<std::string> ret;

  for(SPVInstruction *inst : entries)
    if(inst->entry)
      ret.push_back(inst->entry->name);

  return ret;
}

ShaderStage SPVModule::StageForEntry(const std::string &entryPoint) const
{
  for(SPVInstruction *inst : entries)
  {
    if(inst->entry && inst->entry->name == entryPoint)
    {
      switch(inst->entry->model)
      {
        case spv::ExecutionModelVertex: return ShaderStage::Vertex;
        case spv::ExecutionModelTessellationControl: return ShaderStage::Tess_Control;
        case spv::ExecutionModelTessellationEvaluation: return ShaderStage::Tess_Eval;
        case spv::ExecutionModelGeometry: return ShaderStage::Geometry;
        case spv::ExecutionModelFragment: return ShaderStage::Fragment;
        case spv::ExecutionModelGLCompute: return ShaderStage::Compute;
        case spv::ExecutionModelKernel:
        case spv::ExecutionModelTaskNV:
        case spv::ExecutionModelMeshNV:
        case spv::ExecutionModelRayGenerationNV:
        case spv::ExecutionModelIntersectionNV:
        case spv::ExecutionModelAnyHitNV:
        case spv::ExecutionModelClosestHitNV:
        case spv::ExecutionModelMissNV:
        case spv::ExecutionModelCallableNV:
        case spv::ExecutionModelMax: return ShaderStage::Count;
      }
    }
  }

  return ShaderStage::Count;
}

void SPVModule::MakeReflection(GraphicsAPI sourceAPI, ShaderStage stage,
                               const std::string &entryPoint, ShaderReflection &reflection,
                               ShaderBindpointMapping &mapping, SPIRVPatchData &patchData) const
{
  std::vector<SigParameter> inputs;
  std::vector<SigParameter> outputs;
  std::vector<cblockpair> cblocks;
  std::vector<shaderrespair> samplers, roresources, rwresources;

  // VKTODOLOW filter to only functions/resources used by entryPoint
  reflection.entryPoint = entryPoint;
  reflection.stage = stage;

  // TODO sort these so that the entry point is in the first file
  if(!sourceFiles.empty())
  {
    reflection.debugInfo.encoding = ShaderEncoding::Unknown;
    if(sourceLang == spv::SourceLanguageHLSL)
      reflection.debugInfo.encoding = ShaderEncoding::HLSL;
    else if(sourceLang == spv::SourceLanguageGLSL || sourceLang == spv::SourceLanguageESSL)
      reflection.debugInfo.encoding = ShaderEncoding::GLSL;

    reflection.debugInfo.files.reserve(sourceFiles.size());

    for(size_t i = 0; i < sourceFiles.size(); i++)
    {
      // skip any empty source files
      if(sourceFiles[i].second.empty())
        continue;

      reflection.debugInfo.files.push_back({sourceFiles[i].first, sourceFiles[i].second});
    }
  }

  if(!cmdline.empty())
    reflection.debugInfo.compileFlags.flags = {{"@cmdline", cmdline}};

  // TODO need to fetch these
  reflection.dispatchThreadsDimension[0] = 0;
  reflection.dispatchThreadsDimension[1] = 0;
  reflection.dispatchThreadsDimension[2] = 0;

  ConstantBlock globalsblock;

  for(size_t i = 0; i < globals.size(); i++)
  {
    SPVInstruction *inst = globals[i];
    if(inst->var->storage == spv::StorageClassInput || inst->var->storage == spv::StorageClassOutput)
    {
      bool isInput = inst->var->storage == spv::StorageClassInput;
      std::vector<SigParameter> *sigarray = (isInput ? &inputs : &outputs);

      std::string nm;
      // try to use the instance/variable name
      if(!inst->str.empty())
        nm = inst->str;
      // for structs, if there's no instance name, use the type name
      else if(inst->var->type->type == SPVTypeData::ePointer &&
              inst->var->type->baseType->type == SPVTypeData::eStruct)
        nm = inst->var->type->baseType->name;
      // otherwise fall back to naming after the ID
      else
        nm = StringFormat::Fmt("sig%u", inst->id);

      uint32_t dummy = 0;
      AddSignatureParameter(isInput, stage, inst->id, 0, dummy, std::vector<uint32_t>(), nm,
                            inst->var->type, inst->decorations, *sigarray, patchData);

      // eliminate any members of gl_PerVertex that are actually unused and just came along
      // for the ride (usually with gl_Position, but maybe declared globally and still unused)
      {
        bool globalCheck = false;

        // if this variable is directly decorated, check if it's used
        for(size_t d = 0; d < inst->decorations.size(); d++)
        {
          if(inst->decorations[d].decoration == spv::DecorationBuiltIn)
          {
            spv::BuiltIn builtin = (spv::BuiltIn)inst->decorations[d].val;
            globalCheck = (builtin == spv::BuiltInPointSize || builtin == spv::BuiltInClipDistance ||
                           builtin == spv::BuiltInCullDistance);
            break;
          }
        }

        if(globalCheck)
        {
          bool eliminate = true;

          for(size_t o = 0; o < operations.size(); o++)
          {
            if(operations[o]->op)
            {
              for(size_t a = 0; a < operations[o]->op->arguments.size(); a++)
              {
                if(operations[o]->op->arguments[a] == inst)
                {
                  // we were just looking for any use of this variable
                  eliminate = false;
                  break;
                }
              }
            }

            if(!eliminate)
              break;
          }

          if(eliminate)
          {
            sigarray->pop_back();

            if(isInput)
            {
              if(patchData.inputs.size() > sigarray->size())
                patchData.inputs.pop_back();
            }
            else
            {
              if(patchData.outputs.size() > sigarray->size())
                patchData.outputs.pop_back();
            }
          }
        }

        // if this is a struct (primary case, with gl_PerVertex), check each child for use in an
        // OpAccessChain
        if(inst->var->type->type == SPVTypeData::ePointer &&
           inst->var->type->baseType->type == SPVTypeData::eStruct)
        {
          std::vector<std::vector<SPVDecoration> > &childDecorations =
              inst->var->type->baseType->childDecorations;

          for(size_t c = 0; c < childDecorations.size(); c++)
          {
            spv::BuiltIn checkBuiltin = spv::BuiltInPosition;

            for(size_t d = 0; d < childDecorations[c].size(); d++)
            {
              if(childDecorations[c][d].decoration == spv::DecorationBuiltIn)
              {
                spv::BuiltIn builtin = (spv::BuiltIn)childDecorations[c][d].val;
                if(builtin == spv::BuiltInPointSize || builtin == spv::BuiltInClipDistance ||
                   builtin == spv::BuiltInCullDistance)
                {
                  checkBuiltin = builtin;
                }
                break;
              }
            }

            if(checkBuiltin != spv::BuiltInPosition)
            {
              bool eliminate = true;

              for(size_t o = 0; o < operations.size(); o++)
              {
                // we're only interested in OpAccessChain, which must be used to fetch the
                // child for use. If we find one with the right index, then we're done
                if(operations[o]->op && (operations[o]->opcode == spv::OpAccessChain ||
                                         operations[o]->opcode == spv::OpInBoundsAccessChain))
                {
                  for(size_t a = 0; a < operations[o]->op->arguments.size() - 1; a++)
                  {
                    if(operations[o]->op->arguments[a] == inst)
                    {
                      // check the next argument to see if it's a constant with the
                      // right value

                      // found a use!
                      if(operations[o]->op->arguments[a + 1]->constant &&
                         operations[o]->op->arguments[a + 1]->constant->u32 == (uint32_t)c)
                      {
                        eliminate = false;
                      }
                      break;
                    }
                  }
                }

                if(!eliminate)
                  break;
              }

              if(eliminate)
              {
                ShaderBuiltin attr = BuiltInToSystemAttribute(stage, checkBuiltin);

                // find this builtin in the array, and remove. There might be multiple entries for
                // arrays like gl_ClipDistance
                for(size_t s = 0; s < sigarray->size();)
                {
                  if((*sigarray)[s].systemValue == attr)
                  {
                    sigarray->erase(sigarray->begin() + s);
                    if(isInput)
                      patchData.inputs.erase(patchData.inputs.begin() + s);
                    else
                      patchData.outputs.erase(patchData.outputs.begin() + s);
                  }
                  else
                  {
                    s++;
                  }
                }
              }
            }
          }
        }
      }
    }
    else if(inst->var->storage == spv::StorageClassUniform ||
            inst->var->storage == spv::StorageClassUniformConstant ||
            inst->var->storage == spv::StorageClassPushConstant)
    {
      bool pushConst = (inst->var->storage == spv::StorageClassPushConstant);

      SPVTypeData *type = inst->var->type;
      if(type->type == SPVTypeData::ePointer)
        type = type->baseType;

      bool isArray = false;
      uint32_t arraySize = 1;
      if(type->type == SPVTypeData::eArray)
      {
        isArray = true;
        if(type->arraySize != ~0U)
          arraySize = type->arraySize;
        type = type->baseType;
      }

      if(type->type < SPVTypeData::eCompositeCount)
      {
        // global loose variable - add to $Globals block
        RDCASSERT(type->type != SPVTypeData::ePointer);
        RDCASSERT(sourceAPI == GraphicsAPI::OpenGL);

        ShaderConstant constant;

        MakeConstantBlockVariable(constant, type, inst->str, inst->decorations);

        if(isArray)
          constant.type.descriptor.elements = arraySize;
        else
          constant.type.descriptor.elements = 0;

        for(size_t d = 0; d < inst->decorations.size(); d++)
        {
          if(inst->decorations[d].decoration == spv::DecorationLocation)
            constant.byteOffset = (int32_t)inst->decorations[d].val;
        }

        globalsblock.variables.push_back(constant);
      }
      else if(type->type == SPVTypeData::eStruct)
      {
        ConstantBlock cblock;

        if(!inst->str.empty())
          cblock.name = inst->str;
        else if(!type->name.empty())
          cblock.name = type->name;
        else
          cblock.name = StringFormat::Fmt("uniforms%u", inst->id);
        cblock.bufferBacked = !pushConst;

        Bindpoint bindmap;
        // set can be implicitly 0, but the binding must be set explicitly.
        // If no binding is found, we set INVALID_BIND and sort to the end of the resources
        // list as it's not bound anywhere (most likely, declared but not used)
        bindmap.bind = INVALID_BIND;

        bool ssbo = false;

        ShaderResource res;

        for(size_t d = 0; d < inst->decorations.size(); d++)
        {
          if(inst->decorations[d].decoration == spv::DecorationDescriptorSet)
            bindmap.bindset = (int32_t)inst->decorations[d].val;
          if(inst->decorations[d].decoration == spv::DecorationBinding)
            bindmap.bind = (int32_t)inst->decorations[d].val;
        }

        // set something crazy so this doesn't overlap with a real buffer binding
        if(pushConst)
          bindmap.bindset = 10000;

        for(size_t d = 0; type->decorations && d < type->decorations->size(); d++)
        {
          if((*type->decorations)[d].decoration == spv::DecorationBufferBlock)
            ssbo = true;
        }

        if(ssbo)
        {
          res.isReadOnly = false;
          res.isTexture = false;
          res.name = cblock.name;
          res.resType = TextureType::Buffer;

          res.variableType.descriptor.columns = 0;
          res.variableType.descriptor.rows = 0;
          res.variableType.descriptor.rowMajorStorage = false;
          res.variableType.descriptor.rows = 0;
          res.variableType.descriptor.type = VarType::Float;
          res.variableType.descriptor.name = type->GetName();

          MakeConstantBlockVariables(type, 0, 0, res.variableType.members);
        }
        else
        {
          MakeConstantBlockVariables(type, 0, 0, cblock.variables);

          if(!type->children.empty())
            cblock.byteSize = CalculateMinimumByteSize(cblock.variables);
          else
            cblock.byteSize = 0;
        }

        bindmap.used = false;

        bindmap.arraySize = arraySize;

        for(size_t o = 0; o < operations.size(); o++)
        {
          if(operations[o]->op)
          {
            for(size_t a = 0; a < operations[o]->op->arguments.size(); a++)
            {
              if(operations[o]->op->arguments[a] == inst)
              {
                bindmap.used = true;
                break;
              }
            }
          }
        }

        // we should always have a location. Put that in as the bind, it will be overwritten
        // dynamically with the actual value.
        if(sourceAPI == GraphicsAPI::OpenGL)
        {
          for(size_t d = 0; d < inst->decorations.size(); d++)
          {
            if(inst->decorations[d].decoration == spv::DecorationLocation)
              bindmap.bind = -(int32_t)inst->decorations[d].val;
          }
        }

        // on Vulkan should never have elements that have no binding declared but are used, unless
        // it's push constants (which is handled elsewhere). On GL we should have gotten a location
        // above, which will be rewritten later when looking up the pipeline state since it's
        // mutable from draw to draw in theory.
        RDCASSERT(!bindmap.used || !cblock.bufferBacked || bindmap.bind != INVALID_BIND);

        if(ssbo)
          rwresources.push_back(shaderrespair(bindmap, res));
        else
          cblocks.push_back(cblockpair(bindmap, cblock));
      }
      else
      {
        ShaderResource res;

        res.name = inst->str.empty() ? StringFormat::Fmt("res%u", inst->id) : inst->str;

        SPVTypeData *imageType = type;
        if(imageType->type == SPVTypeData::eSampledImage)
          imageType = type->baseType;

        if(imageType->multisampled)
          res.resType = imageType->arrayed ? TextureType::Texture2DMSArray : TextureType::Texture2DMS;
        else if(imageType->texdim == spv::Dim1D)
          res.resType = imageType->arrayed ? TextureType::Texture1DArray : TextureType::Texture1D;
        else if(imageType->texdim == spv::Dim2D)
          res.resType = imageType->arrayed ? TextureType::Texture2DArray : TextureType::Texture2D;
        else if(imageType->texdim == spv::DimCube)
          res.resType = imageType->arrayed ? TextureType::TextureCubeArray : TextureType::TextureCube;
        else if(imageType->texdim == spv::Dim3D)
          res.resType = TextureType::Texture3D;
        else if(imageType->texdim == spv::DimRect)
          res.resType = TextureType::TextureRect;
        else if(imageType->texdim == spv::DimBuffer)
          res.resType = TextureType::Buffer;

        bool sepSampler = (type->type == SPVTypeData::eSampler);

        res.isTexture = res.resType != TextureType::Buffer && type->type != SPVTypeData::eSampler;
        res.isReadOnly = true;

        SPVTypeData *sampledType = type->baseType;
        if(type->type == SPVTypeData::eSampler)
        {
          res.resType = TextureType::Unknown;
        }
        else if(type->texdim == spv::DimSubpassData)
        {
          res.resType = TextureType::Texture2D;

          res.variableType.descriptor.type = sampledType->GetVarType();
        }
        else
        {
          bool isrw = false;

          if(sampledType->type == SPVTypeData::eImage)
          {
            isrw = (sampledType->sampled == 2);
            sampledType = sampledType->baseType;
          }
          if(type->type == SPVTypeData::eImage)
          {
            isrw = (type->sampled == 2);
          }

          res.isReadOnly = !isrw;

          res.variableType.descriptor.type = sampledType->GetVarType();
        }

        res.variableType.descriptor.rows = 1;
        res.variableType.descriptor.columns = 1;
        res.variableType.descriptor.elements = 1;
        res.variableType.descriptor.rowMajorStorage = false;
        res.variableType.descriptor.rowMajorStorage = false;

        Bindpoint bindmap;
        // set can be implicitly 0, but the binding must be set explicitly.
        // If no binding is found, we set INVALID_BIND and sort to the end of the resources
        // list as it's not bound anywhere (most likely, declared but not used)
        bindmap.bind = INVALID_BIND;

        for(size_t d = 0; d < inst->decorations.size(); d++)
        {
          if(inst->decorations[d].decoration == spv::DecorationDescriptorSet)
            bindmap.bindset = (int32_t)inst->decorations[d].val;
          if(inst->decorations[d].decoration == spv::DecorationBinding)
            bindmap.bind = (int32_t)inst->decorations[d].val;
        }

        bindmap.used = false;

        bindmap.arraySize = arraySize;

        for(size_t o = 0; o < operations.size(); o++)
        {
          if(operations[o]->op)
          {
            for(size_t a = 0; a < operations[o]->op->arguments.size(); a++)
            {
              if(operations[o]->op->arguments[a] == inst)
              {
                bindmap.used = true;
                break;
              }
            }
          }
        }

        // we should always have a location. Put that in as the bind, it will be overwritten
        // dynamically with the actual value.
        if(sourceAPI == GraphicsAPI::OpenGL)
        {
          for(size_t d = 0; d < inst->decorations.size(); d++)
          {
            if(inst->decorations[d].decoration == spv::DecorationLocation)
              bindmap.bind = -(int32_t)inst->decorations[d].val;
          }
        }

        // on Vulkan should never have elements that have no binding declared but are used, unless
        // it's push constants (which is handled elsewhere). On GL we should have gotten a location
        // above, which will be rewritten later when looking up the pipeline state since it's
        // mutable from draw to draw in theory.
        RDCASSERT(!bindmap.used || bindmap.bind != INVALID_BIND);

        if(sepSampler)
          samplers.push_back(shaderrespair(bindmap, res));
        else if(res.isReadOnly)
          roresources.push_back(shaderrespair(bindmap, res));
        else
          rwresources.push_back(shaderrespair(bindmap, res));
      }
    }
    else if(inst->var->storage == spv::StorageClassPrivate ||
            inst->var->storage == spv::StorageClassCrossWorkgroup ||
            inst->var->storage == spv::StorageClassWorkgroup)
    {
      // silently allow
    }
    else
    {
      RDCWARN("Unexpected storage class for global: %s", ToStr(inst->var->storage).c_str());
    }
  }

  if(!specConstants.empty())
  {
    ConstantBlock cblock;

    cblock.name = StringFormat::Fmt("Specialization Constants");
    cblock.bufferBacked = false;
    cblock.byteSize = 0;

    Bindpoint bindmap;

    // set something crazy so this doesn't overlap with a real buffer binding
    // also identify this as specialization constant data
    bindmap.bindset = SpecializationConstantBindSet;
    bindmap.bind = INVALID_BIND;
    bindmap.arraySize = 1;
    bindmap.used = true;

    cblock.variables.resize(specConstants.size());
    for(size_t i = 0; i < specConstants.size(); i++)
    {
      cblock.variables[i].name = specConstants[i]->str;

      MakeConstantBlockVariable(cblock.variables[i], specConstants[i]->constant->type,
                                specConstants[i]->str, specConstants[i]->decorations);

      uint32_t specId = ~0U;

      for(size_t d = 0; d < specConstants[i]->decorations.size(); d++)
      {
        if(specConstants[i]->decorations[d].decoration == spv::DecorationSpecId)
        {
          specId = specConstants[i]->decorations[d].val;
          break;
        }
      }

      if(specId == ~0U)
        RDCERR("Couldn't find specialisation index for spec constant");

      // put the specId in here since we don't have an actual offset for specialization constants.
      cblock.variables[i].byteOffset = specId;
      cblock.variables[i].defaultValue = specConstants[i]->constant->u64;

      RDCASSERTEQUAL(cblock.variables[i].type.members.size(),
                     specConstants[i]->constant->children.size());
      for(size_t c = 0; c < specConstants[i]->constant->children.size(); c++)
      {
        cblock.variables[i].type.members[c].defaultValue =
            specConstants[i]->constant->children[c]->u64;
      }
    }

    cblocks.push_back(cblockpair(bindmap, cblock));
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
  for(SPVInstruction *inst : entries)
  {
    if(inst->entry && inst->entry->name == entryPoint)
    {
      for(const SPVExecutionMode &mode : inst->entry->modes)
      {
        if(mode.mode == spv::ExecutionModeTriangles)
        {
          patchData.outTopo = Topology::TriangleList;
        }
        else if(mode.mode == spv::ExecutionModeIsolines)
        {
          patchData.outTopo = Topology::LineList;
        }
        else if(mode.mode == spv::ExecutionModeOutputPoints)
        {
          patchData.outTopo = Topology::PointList;
        }
        else if(mode.mode == spv::ExecutionModeOutputLineStrip)
        {
          patchData.outTopo = Topology::LineStrip;
        }
        else if(mode.mode == spv::ExecutionModeOutputTriangleStrip)
        {
          patchData.outTopo = Topology::TriangleStrip;
        }
        else if(mode.mode == spv::ExecutionModeQuads)
        {
          patchData.outTopo = Topology::TriangleList;
        }
        else if(mode.mode == spv::ExecutionModeDepthGreater)
        {
          for(SigParameter &sig : outputs)
          {
            if(sig.systemValue == ShaderBuiltin::DepthOutput)
              sig.systemValue = ShaderBuiltin::DepthOutputGreaterEqual;
          }
          break;
        }
        else if(mode.mode == spv::ExecutionModeDepthLess)
        {
          for(SigParameter &sig : outputs)
          {
            if(sig.systemValue == ShaderBuiltin::DepthOutput)
              sig.systemValue = ShaderBuiltin::DepthOutputLessEqual;
          }
          break;
        }
      }

      break;
    }
  }

  // sort system value semantics to the start of the list
  struct sig_param_sort
  {
    sig_param_sort(const std::vector<SigParameter> &arr) : sigArray(arr) {}
    const std::vector<SigParameter> &sigArray;

    bool operator()(const size_t idxA, const size_t idxB)
    {
      const SigParameter &a = sigArray[idxA];
      const SigParameter &b = sigArray[idxB];

      if(a.systemValue == b.systemValue)
      {
        if(a.regIndex != b.regIndex)
          return a.regIndex < b.regIndex;

        return a.varName < b.varName;
      }
      if(a.systemValue == ShaderBuiltin::Undefined)
        return false;
      if(b.systemValue == ShaderBuiltin::Undefined)
        return true;

      return a.systemValue < b.systemValue;
    }
  };

  std::vector<size_t> indices;
  {
    indices.resize(inputs.size());
    for(size_t i = 0; i < inputs.size(); i++)
      indices[i] = i;

    std::sort(indices.begin(), indices.end(), sig_param_sort(inputs));

    reflection.inputSignature.reserve(inputs.size());
    for(size_t i = 0; i < inputs.size(); i++)
      reflection.inputSignature.push_back(inputs[indices[i]]);

    std::vector<SPIRVPatchData::InterfaceAccess> inPatch = patchData.inputs;
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

    std::vector<SPIRVPatchData::InterfaceAccess> outPatch = patchData.outputs;
    for(size_t i = 0; i < outputs.size(); i++)
      patchData.outputs[i] = outPatch[indices[i]];
  }

  size_t numInputs = 16;

  for(size_t i = 0; i < reflection.inputSignature.size(); i++)
    if(reflection.inputSignature[i].systemValue == ShaderBuiltin::Undefined)
      numInputs = RDCMAX(numInputs, (size_t)reflection.inputSignature[i].regIndex + 1);

  mapping.inputAttributes.resize(numInputs);
  for(size_t i = 0; i < numInputs; i++)
    mapping.inputAttributes[i] = INVALID_BIND;

  for(size_t i = 0; i < reflection.inputSignature.size(); i++)
    if(reflection.inputSignature[i].systemValue == ShaderBuiltin::Undefined)
      mapping.inputAttributes[reflection.inputSignature[i].regIndex] = (int32_t)i;

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
}

void ParseSPIRV(uint32_t *spirv, size_t spirvLength, SPVModule &module)
{
  if(spirv[0] != (uint32_t)spv::MagicNumber)
  {
    RDCERR("Unrecognised SPIR-V magic number %08x", spirv[0]);
    return;
  }

  uint32_t packedVersion = spirv[1];

  // Bytes: 0 | major | minor | 0
  module.moduleVersion.major = uint8_t((packedVersion & 0x00ff0000) >> 16);
  module.moduleVersion.minor = uint8_t((packedVersion & 0x0000ff00) >> 8);

  // support 1.0 to 1.3, don't care about if the API would support it.
  if(packedVersion < 0x00010000 || packedVersion > 0x00010300)
  {
    RDCERR("Unsupported SPIR-V version: %08x", spirv[1]);
    return;
  }

  module.spirv.assign(spirv, spirv + spirvLength);

  module.generator = spirv[2];

  bool hasParamsInModuleProcessed = false;

  {
    uint32_t toolid = (module.generator & 0xffff0000) >> 16;

    // see table above:
    //  - 8 is glslang
    //  - 13 is shaderc
    //  - 14 is dxc

    if(toolid == 8 || toolid == 13 || toolid == 14)
      hasParamsInModuleProcessed = true;
  }

  uint32_t idbound = spirv[3];
  module.ids.resize(idbound);

  RDCASSERT(spirv[4] == 0);

  SPVFunction *curFunc = NULL;
  SPVBlock *curBlock = NULL;

  size_t it = 5;
  while(it < spirvLength)
  {
    uint16_t WordCount = spirv[it] >> spv::WordCountShift;

    module.operations.push_back(new SPVInstruction());
    SPVInstruction &op = *module.operations.back();

    op.opcode = spv::Op(spirv[it] & spv::OpCodeMask);

    bool mathop = false;

    switch(op.opcode)
    {
      //////////////////////////////////////////////////////////////////////
      // 'Global' opcodes
      case spv::OpSource:
      {
        module.sourceLang = spv::SourceLanguage(spirv[it + 1]);
        module.sourceVer = spirv[it + 2];

        if(WordCount > 4)
        {
          rdcpair<std::string, std::string> sourceFile;

          SPVInstruction *filenameInst = module.GetByID(spirv[it + 3]);
          RDCASSERT(filenameInst);

          sourceFile.first = filenameInst->str;

          std::string source = (const char *)&spirv[it + 4];

          // glslang outputs command-line arguments as OpModuleProcessed - before SPIR-V 1.1 where
          // it was an actual op, it was output as comments in the source.
          if(hasParamsInModuleProcessed)
          {
            const char compileFlagPrefix[] = "// OpModuleProcessed ";
            const char endMarker[] = "#line 1\n";
            if(source.find(compileFlagPrefix) == 0)
            {
              // process compile flags
              size_t nextLine = source.find('\n');
              while(nextLine != std::string::npos)
              {
                bool finished = false;
                if(source.find(compileFlagPrefix) == 0)
                {
                  size_t offs = sizeof(compileFlagPrefix) - 1;
                  module.cmdline += " --" + source.substr(offs, nextLine - offs);
                }
                else if(source.find(endMarker) == 0)
                {
                  finished = true;
                }
                else
                {
                  RDCERR("Unexpected preamble line with OpModuleProcessed: %s",
                         std::string(source.begin(), source.begin() + nextLine).c_str());
                  break;
                }

                // erase this line
                source.erase(0, nextLine + 1);

                nextLine = source.find('\n');

                if(finished)
                  break;
              }
            }
          }

          sourceFile.second = source;

          module.sourceFiles.push_back(sourceFile);
        }
        else if(WordCount > 3)
        {
          RDCWARN("Only filename provided in OpSource, being discarded without source code");
        }

        break;
      }
      case spv::OpSourceContinued:
      {
        if(!module.sourceFiles.empty())
        {
          rdcpair<std::string, std::string> &sourceFile = module.sourceFiles.back();

          sourceFile.second += (const char *)&spirv[it + 1];
        }
        else
        {
          RDCERR("OpSourceContinued without matching OpSource");
        }
        break;
      }
      case spv::OpSourceExtension:
      {
        op.str = (const char *)&spirv[it + 1];
        module.sourceexts.push_back(&op);
        break;
      }
      case spv::OpModuleProcessed:
      {
        // glslang outputs command-line arguments as OpModuleProcessed
        if(hasParamsInModuleProcessed)
        {
          module.cmdline += " --";
          module.cmdline += (const char *)&spirv[it + 1];
        }
        break;
      }
      case spv::OpExtension:
      {
        std::string ext = (const char *)&spirv[it + 1];
        module.extensions.push_back(ext);
        break;
      }
      case spv::OpCapability:
      {
        module.capabilities.push_back((spv::Capability)spirv[it + 1]);
        break;
      }
      case spv::OpMemoryModel:
      {
        // do we care about this?
        spv::AddressingModel addr = spv::AddressingModel(spirv[it + 1]);
        spv::MemoryModel mem = spv::MemoryModel(spirv[it + 2]);

        (void)addr;
        (void)mem;

        break;
      }
      case spv::OpEntryPoint:
      {
        op.entry = new SPVEntryPoint();
        op.entry->func = spirv[it + 2];
        op.entry->model = spv::ExecutionModel(spirv[it + 1]);
        op.entry->name = (const char *)&spirv[it + 3];

        // VKTODOLOW look at interface IDs?
        module.entries.push_back(&op);
        break;
      }
      case spv::OpExecutionMode:
      {
        uint32_t func = spirv[it + 1];
        for(size_t e = 0; e < module.entries.size(); e++)
        {
          if(module.entries[e]->entry->func == func)
          {
            SPVExecutionMode mode;
            mode.mode = (spv::ExecutionMode)spirv[it + 2];

            if(WordCount > 3)
              mode.x = spirv[it + 3];
            if(WordCount > 4)
              mode.y = spirv[it + 4];
            if(WordCount > 5)
              mode.z = spirv[it + 5];

            module.entries[e]->entry->modes.push_back(mode);
            break;
          }
        }
        break;
      }
      case spv::OpExtInstImport:
      {
        op.ext = new SPVExtInstSet();
        op.ext->setname = (const char *)&spirv[it + 2];
        op.ext->canonicalNames = NULL;

        if(op.ext->setname == "GLSL.std.450")
        {
          op.ext->canonicalNames = GLSL_STD_450_names;
          op.ext->friendlyNames = GLSL_STD_450_friendly_names;
        }

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpString:
      {
        op.str = (const char *)&spirv[it + 2];

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      //////////////////////////////////////////////////////////////////////
      // Type opcodes
      case spv::OpTypeVoid:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eVoid;

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeBool:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eBool;

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeInt:
      {
        op.type = new SPVTypeData();
        op.type->type = spirv[it + 3] ? SPVTypeData::eSInt : SPVTypeData::eUInt;
        op.type->bitCount = spirv[it + 2];

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeFloat:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eFloat;
        op.type->bitCount = spirv[it + 2];

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeVector:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eVector;

        SPVInstruction *baseTypeInst = module.GetByID(spirv[it + 2]);
        RDCASSERT(baseTypeInst && baseTypeInst->type);

        op.type->baseType = baseTypeInst->type;
        op.type->vectorSize = spirv[it + 3];

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeMatrix:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eMatrix;

        SPVInstruction *baseTypeInst = module.GetByID(spirv[it + 2]);
        RDCASSERT(baseTypeInst && baseTypeInst->type);

        RDCASSERT(baseTypeInst->type->type == SPVTypeData::eVector);

        op.type->baseType = baseTypeInst->type->baseType;
        op.type->vectorSize = baseTypeInst->type->vectorSize;
        op.type->matrixSize = spirv[it + 3];

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeArray:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eArray;

        SPVInstruction *baseTypeInst = module.GetByID(spirv[it + 2]);
        RDCASSERT(baseTypeInst && baseTypeInst->type);

        op.type->baseType = baseTypeInst->type;

        SPVInstruction *sizeInst = module.GetByID(spirv[it + 3]);
        RDCASSERT(sizeInst && sizeInst->constant && sizeInst->constant->type->IsBasicInt());

        op.type->arraySizeConst = sizeInst->constant;
        op.type->arraySize = sizeInst->constant->EvaluateIntValue();

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeRuntimeArray:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eArray;

        SPVInstruction *baseTypeInst = module.GetByID(spirv[it + 2]);
        RDCASSERT(baseTypeInst && baseTypeInst->type);

        op.type->baseType = baseTypeInst->type;
        op.type->arraySize = ~0U;

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeStruct:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eStruct;
        op.type->id = spirv[it + 1];

        for(int i = 2; i < WordCount; i++)
        {
          SPVInstruction *memberInst = module.GetByID(spirv[it + i]);
          RDCASSERT(memberInst && memberInst->type);

          // names might come later from OpMemberName instructions
          op.type->children.push_back({memberInst->type, ""});
          op.type->childDecorations.push_back(std::vector<SPVDecoration>());
        }

        module.structs.push_back(&op);

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypePointer:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::ePointer;

        SPVInstruction *baseTypeInst = module.GetByID(spirv[it + 3]);
        RDCASSERT(baseTypeInst && baseTypeInst->type);

        op.type->baseType = baseTypeInst->type;
        op.type->storage = spv::StorageClass(spirv[it + 2]);

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeImage:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eImage;

        SPVInstruction *baseTypeInst = module.GetByID(spirv[it + 2]);
        RDCASSERT(baseTypeInst && baseTypeInst->type);

        op.type->baseType = baseTypeInst->type;

        op.type->texdim = spv::Dim(spirv[it + 3]);
        op.type->depth = spirv[it + 4] != 0;
        op.type->arrayed = spirv[it + 5] != 0;
        op.type->multisampled = spirv[it + 6] != 0;
        op.type->sampled = spirv[it + 7];
        op.type->imgformat = spv::ImageFormat(spirv[it + 8]);

        // not checking access qualifier

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeSampler:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eSampler;

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeSampledImage:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eSampledImage;

        SPVInstruction *baseTypeInst = module.GetByID(spirv[it + 2]);
        RDCASSERT(baseTypeInst && baseTypeInst->type);

        op.type->baseType = baseTypeInst->type;

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpTypeFunction:
      {
        op.type = new SPVTypeData();
        op.type->type = SPVTypeData::eFunction;

        for(int i = 3; i < WordCount; i++)
        {
          SPVInstruction *argInst = module.GetByID(spirv[it + i]);
          RDCASSERT(argInst && argInst->type);

          // function parameters have no name
          op.type->children.push_back({argInst->type, ""});
          op.type->childDecorations.push_back(std::vector<SPVDecoration>());
        }

        SPVInstruction *baseTypeInst = module.GetByID(spirv[it + 2]);
        RDCASSERT(baseTypeInst && baseTypeInst->type);

        // return type
        op.type->baseType = baseTypeInst->type;

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      //////////////////////////////////////////////////////////////////////
      // Constants
      case spv::OpSpecConstantTrue:
      case spv::OpSpecConstantFalse:
      case spv::OpConstantTrue:
      case spv::OpConstantFalse:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.constant = new SPVConstant();
        op.constant->specialized =
            (op.opcode == spv::OpSpecConstantTrue || op.opcode == spv::OpSpecConstantFalse);
        op.constant->type = typeInst->type;

        op.constant->u32 =
            (op.opcode == spv::OpConstantTrue || op.opcode == spv::OpSpecConstantTrue) ? 1 : 0;

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;
        break;
      }
      case spv::OpConstantNull:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.constant = new SPVConstant();
        op.constant->type = typeInst->type;

        op.constant->u32 = 0;

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;
        break;
      }
      case spv::OpSpecConstant:
      case spv::OpConstant:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.constant = new SPVConstant();
        op.constant->specialized = op.opcode == spv::OpSpecConstant;
        op.constant->type = typeInst->type;

        op.constant->u32 = spirv[it + 3];

        if(WordCount > 3)
        {
          // only handle 32-bit or 64-bit constants
          RDCASSERT(WordCount <= 5);

          uint64_t lo = spirv[it + 3];
          uint64_t hi = WordCount == 5 ? spirv[it + 4] : 0;

          op.constant->u64 = lo | (hi << 32);
        }

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;
        break;
      }
      case spv::OpSpecConstantComposite:
      case spv::OpConstantComposite:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.constant = new SPVConstant();
        op.constant->specialized = op.opcode == spv::OpSpecConstantComposite;
        op.constant->type = typeInst->type;

        for(int i = 3; i < WordCount; i++)
        {
          SPVInstruction *constInst = module.GetByID(spirv[it + i]);
          RDCASSERT(constInst && constInst->constant);

          op.constant->children.push_back(constInst->constant);
        }

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;

        break;
      }
      case spv::OpConstantSampler:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.constant = new SPVConstant();
        op.constant->type = typeInst->type;

        op.constant->sampler.addressing = spv::SamplerAddressingMode(spirv[it + 3]);
        op.constant->sampler.normalised = (spirv[it + 4] != 0);
        op.constant->sampler.filter = spv::SamplerFilterMode(spirv[it + 5]);

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;

        break;
      }
      case spv::OpSpecConstantOp:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.constant = new SPVConstant();
        op.constant->specialized = true;
        op.constant->type = typeInst->type;

        op.constant->specOp = (spv::Op)spirv[it + 3];

        for(int i = 4; i < WordCount; i++)
        {
          SPVInstruction *constInst = module.GetByID(spirv[it + i]);
          RDCASSERT(constInst && constInst->constant);

          op.constant->children.push_back(constInst->constant);
        }

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;

        break;
      }
      //////////////////////////////////////////////////////////////////////
      // Functions
      case spv::OpFunction:
      {
        SPVInstruction *retTypeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(retTypeInst && retTypeInst->type);

        SPVInstruction *typeInst = module.GetByID(spirv[it + 4]);
        RDCASSERT(typeInst && typeInst->type);

        op.func = new SPVFunction();
        op.func->retType = retTypeInst->type;
        op.func->funcType = typeInst->type;
        op.func->control = spv::FunctionControlMask(spirv[it + 3]);

        module.funcs.push_back(&op);

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;

        curFunc = op.func;

        break;
      }
      case spv::OpFunctionEnd:
      {
        curFunc = NULL;
        break;
      }
      //////////////////////////////////////////////////////////////////////
      // Variables
      case spv::OpUndef:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        // no parameters, will be disassembled as appropriate

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;
        break;
      }
      case spv::OpVariable:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.var = new SPVVariable();
        op.var->type = typeInst->type;
        op.var->storage = spv::StorageClass(spirv[it + 3]);

        if(WordCount > 4)
        {
          SPVInstruction *initInst = module.GetByID(spirv[it + 4]);
          RDCASSERT(initInst && initInst->constant);
          op.var->initialiser = initInst->constant;
        }

        if(curFunc)
          curFunc->variables.push_back(&op);
        else
          module.globals.push_back(&op);

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;
        break;
      }
      case spv::OpFunctionParameter:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.var = new SPVVariable();
        op.var->type = typeInst->type;
        op.var->storage = spv::StorageClassFunction;

        RDCASSERT(curFunc);
        curFunc->arguments.push_back(&op);

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;
        break;
      }
      //////////////////////////////////////////////////////////////////////
      // Branching/flow control
      case spv::OpLabel:
      {
        op.block = new SPVBlock();

        RDCASSERT(curFunc);

        curFunc->blocks.push_back(&op);
        curBlock = op.block;

        op.id = spirv[it + 1];
        module.ids[spirv[it + 1]] = &op;
        break;
      }
      case spv::OpKill:
      case spv::OpUnreachable:
      case spv::OpReturn:
      {
        op.flow = new SPVFlowControl();

        curBlock->exitFlow = &op;
        curBlock = NULL;
        break;
      }
      case spv::OpReturnValue:
      {
        op.flow = new SPVFlowControl();

        op.flow->targets.push_back(spirv[it + 1]);

        curBlock->exitFlow = &op;
        curBlock = NULL;
        break;
      }
      case spv::OpBranch:
      {
        op.flow = new SPVFlowControl();

        op.flow->targets.push_back(spirv[it + 1]);

        curBlock->exitFlow = &op;
        curBlock = NULL;
        break;
      }
      case spv::OpBranchConditional:
      {
        op.flow = new SPVFlowControl();

        SPVInstruction *condInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(condInst);

        op.flow->condition = condInst;
        op.flow->targets.push_back(spirv[it + 2]);
        op.flow->targets.push_back(spirv[it + 3]);

        if(WordCount == 6)
        {
          op.flow->literals.push_back(spirv[it + 4]);
          op.flow->literals.push_back(spirv[it + 5]);
        }

        curBlock->exitFlow = &op;
        curBlock = NULL;
        break;
      }
      case spv::OpSwitch:
      {
        op.flow = new SPVFlowControl();

        SPVInstruction *condInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(condInst);

        op.flow->condition = condInst;

        if(condInst->op)
          RDCASSERT(condInst->op->type->IsBasicInt() && condInst->op->type->bitCount <= 32);
        if(condInst->var)
          RDCASSERT(condInst->var->type->IsBasicInt() && condInst->var->type->bitCount <= 32);

        for(int i = 3; i < WordCount; i += 2)
        {
          op.flow->literals.push_back(spirv[it + i + 0]);
          op.flow->targets.push_back(spirv[it + i + 1]);
        }

        // first target is always the default
        op.flow->targets.push_back(spirv[it + 2]);

        curBlock->exitFlow = &op;
        curBlock = NULL;
        break;
      }
      case spv::OpSelectionMerge:
      {
        op.flow = new SPVFlowControl();

        op.flow->targets.push_back(spirv[it + 1]);
        op.flow->selControl = spv::SelectionControlMask(spirv[it + 2]);

        curBlock->mergeFlow = &op;
        break;
      }
      case spv::OpLoopMerge:
      {
        op.flow = new SPVFlowControl();

        op.flow->targets.push_back(spirv[it + 1]);
        op.flow->loopControl = spv::LoopControlMask(spirv[it + 2]);

        curBlock->mergeFlow = &op;
        break;
      }
      //////////////////////////////////////////////////////////////////////
      // Operations with special parameters
      case spv::OpLoad:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.op = new SPVOperation();
        op.op->type = typeInst->type;

        SPVInstruction *ptrInst = module.GetByID(spirv[it + 3]);
        RDCASSERT(ptrInst);

        op.op->arguments.push_back(ptrInst);

        op.op->access = spv::MemoryAccessMaskNone;
        if(WordCount > 4)
          op.op->access = spv::MemoryAccessMask(spirv[it + 4]);

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpStore:
      case spv::OpCopyMemory:
      {
        op.op = new SPVOperation();
        op.op->type = NULL;

        SPVInstruction *ptrInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(ptrInst);

        SPVInstruction *valInst = module.GetByID(spirv[it + 2]);
        RDCASSERT(valInst);

        op.op->arguments.push_back(ptrInst);
        op.op->arguments.push_back(valInst);

        op.op->access = spv::MemoryAccessMaskNone;
        if(WordCount > 3)
          op.op->access = spv::MemoryAccessMask(spirv[it + 3]);

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpPhi:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.op = new SPVOperation();
        op.op->type = typeInst->type;

        for(int i = 3; i < WordCount; i += 2)
        {
          SPVInstruction *varInst = module.GetByID(spirv[it + i + 0]);
          SPVInstruction *blockInst = module.GetByID(spirv[it + i + 1]);
          RDCASSERT(varInst);
          op.op->arguments.push_back(varInst);

          // need the arguments to the OpPhi non-inlined
          if(varInst && varInst->op)
            varInst->op->complexity = NEVER_INLINE_COMPLEXITY;

          // could we use blockInst somehow?
          (void)blockInst;
        }

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpImageTexelPointer:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.op = new SPVOperation();
        op.op->type = typeInst->type;

        SPVInstruction *imageInst = module.GetByID(spirv[it + 3]);
        RDCASSERT(imageInst);

        SPVInstruction *coordInst = module.GetByID(spirv[it + 4]);
        RDCASSERT(coordInst);

        SPVInstruction *sampleInst = module.GetByID(spirv[it + 5]);
        RDCASSERT(sampleInst);

        op.op->arguments.push_back(imageInst);
        op.op->arguments.push_back(coordInst);
        op.op->arguments.push_back(sampleInst);

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;

        // never combine this as it's like a variable declaration
        op.op->complexity = NEVER_INLINE_COMPLEXITY;

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpImageSampleImplicitLod:
      case spv::OpImageSampleExplicitLod:
      case spv::OpImageSampleDrefImplicitLod:
      case spv::OpImageSampleDrefExplicitLod:
      case spv::OpImageSampleProjImplicitLod:
      case spv::OpImageSampleProjExplicitLod:
      case spv::OpImageSampleProjDrefImplicitLod:
      case spv::OpImageSampleProjDrefExplicitLod:
      // these only vary from the above by return type, which doesn't change
      // their disassembly
      case spv::OpImageSparseSampleImplicitLod:
      case spv::OpImageSparseSampleExplicitLod:
      case spv::OpImageSparseSampleDrefImplicitLod:
      case spv::OpImageSparseSampleDrefExplicitLod:
      case spv::OpImageSparseSampleProjImplicitLod:
      case spv::OpImageSparseSampleProjExplicitLod:
      case spv::OpImageSparseSampleProjDrefImplicitLod:
      case spv::OpImageSparseSampleProjDrefExplicitLod:
      // similarly the image operations are very close
      case spv::OpImageFetch:
      case spv::OpImageGather:
      case spv::OpImageDrefGather:
      case spv::OpImageRead:
      case spv::OpImageWrite:
      case spv::OpImageSparseFetch:
      case spv::OpImageSparseGather:
      case spv::OpImageSparseDrefGather:
      case spv::OpImageSparseRead:
      {
        uint32_t idx = 1;

        SPVInstruction *typeInst = NULL;

        if(op.opcode != spv::OpImageWrite)
        {
          typeInst = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(typeInst && typeInst->type);
        }

        // bucket the different opcodes
        bool implicit = false, dref = false, image = false;
        switch(op.opcode)
        {
          case spv::OpImageSampleImplicitLod:
          case spv::OpImageSampleDrefImplicitLod:
          case spv::OpImageSampleProjImplicitLod:
          case spv::OpImageSampleProjDrefImplicitLod:
          case spv::OpImageSparseSampleImplicitLod:
          case spv::OpImageSparseSampleDrefImplicitLod:
          case spv::OpImageSparseSampleProjImplicitLod:
          case spv::OpImageSparseSampleProjDrefImplicitLod: implicit = true; break;
          default: break;
        }
        switch(op.opcode)
        {
          case spv::OpImageFetch:
          case spv::OpImageGather:
          case spv::OpImageDrefGather:
          case spv::OpImageRead:
          case spv::OpImageWrite:
          case spv::OpImageSparseFetch:
          case spv::OpImageSparseGather:
          case spv::OpImageSparseDrefGather:
          case spv::OpImageSparseRead: image = true; break;
          default: break;
        }
        switch(op.opcode)
        {
          case spv::OpImageSampleDrefImplicitLod:
          case spv::OpImageSampleDrefExplicitLod:
          case spv::OpImageSampleProjDrefImplicitLod:
          case spv::OpImageSampleProjDrefExplicitLod:
          case spv::OpImageDrefGather:
          case spv::OpImageSparseSampleDrefImplicitLod:
          case spv::OpImageSparseSampleDrefExplicitLod:
          case spv::OpImageSparseSampleProjDrefImplicitLod:
          case spv::OpImageSparseSampleProjDrefExplicitLod:
          case spv::OpImageSparseDrefGather: dref = true; break;
          default: break;
        }

        op.op = new SPVOperation();

        if(op.opcode != spv::OpImageWrite)
        {
          op.op->type = typeInst->type;
          op.id = spirv[it + idx];
          idx++;
          module.ids[op.id] = &op;
        }

        // sampled image
        {
          SPVInstruction *argInst = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(argInst);

          op.op->arguments.push_back(argInst);
        }

        // co-ords
        {
          SPVInstruction *argInst = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(argInst);

          op.op->arguments.push_back(argInst);
        }

        // Dref (depth reference), gather (component) and write (written value)
        // have an extra value
        if(dref || op.opcode == spv::OpImageGather || op.opcode == spv::OpImageWrite)
        {
          SPVInstruction *argInst = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(argInst);

          op.op->arguments.push_back(argInst);
        }

        // const argument bitfield
        uint32_t imMask = 0;
        if(WordCount > idx)
        {
          imMask = spirv[it + idx];
          idx++;
        }

        // explicit lod sample instructions must pass a lod or grad argument
        if(!implicit && !image)
          RDCASSERT(imMask & (spv::ImageOperandsLodMask | spv::ImageOperandsGradMask));

        // optional arguments

        if(imMask & spv::ImageOperandsBiasMask)
        {
          RDCASSERT(WordCount > idx);
          RDCASSERT(implicit);
          op.op->im.bias = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(op.op->im.bias);
          op.op->arguments.push_back(op.op->im.bias);
        }

        if(imMask & spv::ImageOperandsLodMask)
        {
          RDCASSERT(WordCount > idx);
          RDCASSERT(!implicit);
          op.op->im.lod = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(op.op->im.lod);
          op.op->arguments.push_back(op.op->im.lod);
        }

        if(imMask & spv::ImageOperandsGradMask)
        {
          RDCASSERT(WordCount > idx + 1);
          RDCASSERT(!implicit);
          op.op->im.dx = module.GetByID(spirv[it + idx]);
          idx++;
          op.op->im.dy = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(op.op->im.dx && op.op->im.dy);
          op.op->arguments.push_back(op.op->im.dx);
          op.op->arguments.push_back(op.op->im.dy);
        }

        if(imMask & spv::ImageOperandsConstOffsetMask)
        {
          RDCASSERT(WordCount > idx);
          op.op->im.constOffset = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(op.op->im.constOffset);
          op.op->arguments.push_back(op.op->im.constOffset);
        }

        if(imMask & spv::ImageOperandsOffsetMask)
        {
          RDCASSERT(WordCount > idx);
          op.op->im.offset = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(op.op->im.offset);
          op.op->arguments.push_back(op.op->im.offset);
        }

        if(imMask & spv::ImageOperandsConstOffsetsMask)
        {
          RDCASSERT(WordCount > idx);
          RDCASSERT(op.opcode == spv::OpImageGather || op.opcode == spv::OpImageDrefGather);
          op.op->im.gatherOffsets = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(op.op->im.gatherOffsets);
          op.op->arguments.push_back(op.op->im.gatherOffsets);
        }

        if(imMask & spv::ImageOperandsSampleMask)
        {
          RDCASSERT(WordCount > idx);
          RDCASSERT(op.opcode == spv::OpImageFetch || op.opcode == spv::OpImageRead ||
                    op.opcode == spv::OpImageWrite);
          op.op->im.sampleIdx = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(op.op->im.sampleIdx);
          op.op->arguments.push_back(op.op->im.sampleIdx);
        }

        if(imMask & spv::ImageOperandsMinLodMask)
        {
          RDCASSERT(WordCount > idx);
          op.op->im.minLod = module.GetByID(spirv[it + idx]);
          idx++;
          RDCASSERT(op.op->im.minLod);
          op.op->arguments.push_back(op.op->im.minLod);
        }

        curBlock->instructions.push_back(&op);
        break;
      }
      // any operations that just take N IDs as parameters can be treated the same way
      case spv::OpIAdd:
      case spv::OpFAdd:
      case spv::OpISub:
      case spv::OpFSub:
      case spv::OpIMul:
      case spv::OpFMul:
      case spv::OpFDiv:
      case spv::OpUDiv:
      case spv::OpSDiv:
      case spv::OpFMod:
      case spv::OpUMod:
      case spv::OpSMod:
      case spv::OpFRem:
      case spv::OpSRem:
      case spv::OpVectorTimesScalar:
      case spv::OpMatrixTimesScalar:
      case spv::OpMatrixTimesVector:
      case spv::OpVectorTimesMatrix:
      case spv::OpMatrixTimesMatrix:
      case spv::OpIEqual:
      case spv::OpINotEqual:
      case spv::OpSLessThan:
      case spv::OpSLessThanEqual:
      case spv::OpSGreaterThan:
      case spv::OpSGreaterThanEqual:
      case spv::OpULessThan:
      case spv::OpULessThanEqual:
      case spv::OpUGreaterThan:
      case spv::OpUGreaterThanEqual:
      case spv::OpFOrdEqual:
      case spv::OpFOrdNotEqual:
      case spv::OpFOrdLessThan:
      case spv::OpFOrdLessThanEqual:
      case spv::OpFOrdGreaterThan:
      case spv::OpFOrdGreaterThanEqual:
      case spv::OpFUnordEqual:
      case spv::OpFUnordNotEqual:
      case spv::OpFUnordLessThan:
      case spv::OpFUnordLessThanEqual:
      case spv::OpFUnordGreaterThan:
      case spv::OpFUnordGreaterThanEqual:
      case spv::OpLogicalAnd:
      case spv::OpLogicalOr:
      case spv::OpLogicalEqual:
      case spv::OpLogicalNotEqual:
      case spv::OpBitwiseAnd:
      case spv::OpBitwiseOr:
      case spv::OpBitwiseXor:
      case spv::OpShiftLeftLogical:
      case spv::OpShiftRightLogical:
      case spv::OpShiftRightArithmetic:

      case spv::OpFNegate:
      case spv::OpSNegate:
      case spv::OpNot:
      case spv::OpLogicalNot:
        mathop = true;    // deliberate fallthrough

      case spv::OpCompositeConstruct:
      case spv::OpAccessChain:
      case spv::OpInBoundsAccessChain:
      case spv::OpDot:
      case spv::OpSelect:
      case spv::OpConvertFToS:
      case spv::OpConvertFToU:
      case spv::OpConvertUToF:
      case spv::OpConvertSToF:
      case spv::OpQuantizeToF16:
      case spv::OpFConvert:
      case spv::OpUConvert:
      case spv::OpSConvert:
      case spv::OpBitcast:
      case spv::OpBitFieldInsert:
      case spv::OpBitFieldSExtract:
      case spv::OpBitFieldUExtract:
      case spv::OpBitReverse:
      case spv::OpBitCount:
      case spv::OpAny:
      case spv::OpAll:
      case spv::OpIsNan:
      case spv::OpIsInf:
      case spv::OpOuterProduct:
      case spv::OpTranspose:
      case spv::OpCopyObject:
      case spv::OpDPdx:
      case spv::OpDPdy:
      case spv::OpFwidth:
      case spv::OpDPdxFine:
      case spv::OpDPdyFine:
      case spv::OpFwidthFine:
      case spv::OpDPdxCoarse:
      case spv::OpDPdyCoarse:
      case spv::OpFwidthCoarse:
      case spv::OpImageSparseTexelsResident:
      case spv::OpImage:
      case spv::OpSampledImage:
      case spv::OpImageQuerySizeLod:
      case spv::OpImageQuerySize:
      case spv::OpImageQueryLod:
      case spv::OpImageQueryLevels:
      case spv::OpImageQuerySamples:
      case spv::OpFunctionCall:
      {
        int word = 1;

        SPVInstruction *typeInst = module.GetByID(spirv[it + word]);
        RDCASSERT(typeInst && typeInst->type);

        word++;

        op.op = new SPVOperation();
        op.op->type = typeInst->type;
        op.op->mathop = mathop;

        op.id = spirv[it + word];
        module.ids[spirv[it + word]] = &op;

        word++;

        if(op.opcode == spv::OpFunctionCall)
        {
          // never combine function calls. It can sometimes be nice, but since
          // we can combine multiple times and function calls have side-effects,
          // it can appear to change the meaning of the code.
          op.op->complexity = NEVER_INLINE_COMPLEXITY;

          op.op->funcCall = spirv[it + word];

          word++;
        }

        for(; word < WordCount; word++)
        {
          SPVInstruction *argInst = module.GetByID(spirv[it + word]);
          RDCASSERT(argInst);

          op.op->arguments.push_back(argInst);
        }

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpEmitVertex:
      case spv::OpEmitStreamVertex:
      case spv::OpEndPrimitive:
      case spv::OpEndStreamPrimitive:
      {
        // these don't emit an ID, don't take a type, they are just
        // single operations
        op.op = new SPVOperation();
        op.op->type = NULL;

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpControlBarrier:
      case spv::OpMemoryBarrier:
      {
        // these don't emit an ID, just have some properties
        op.op = new SPVOperation();
        op.op->type = NULL;

        int word = 1;

        SPVInstruction *scopeInst = module.GetByID(spirv[it + word]);
        RDCASSERT(scopeInst &&
                  scopeInst->constant);    // shader capability requires this to be a constant

        if(scopeInst && scopeInst->constant)
          op.op->scope = (spv::Scope)scopeInst->constant->u32;

        word++;

        // control barrier specifies two scopes, execution and memory
        if(op.opcode == spv::OpControlBarrier)
        {
          scopeInst = module.GetByID(spirv[it + word]);
          RDCASSERT(scopeInst &&
                    scopeInst->constant);    // shader capability requires this to be a constant

          if(scopeInst && scopeInst->constant)
            op.op->scopeMemory = (spv::Scope)scopeInst->constant->u32;

          word++;
        }

        SPVInstruction *semanticsInst = module.GetByID(spirv[it + word]);
        RDCASSERT(semanticsInst &&
                  semanticsInst->constant);    // shader capability requires this to be a constant

        if(semanticsInst && semanticsInst->constant)
          op.op->semantics = (spv::MemorySemanticsMask)semanticsInst->constant->u32;

        word++;

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpVectorShuffle:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.op = new SPVOperation();
        op.op->type = typeInst->type;

        {
          SPVInstruction *argInst = module.GetByID(spirv[it + 3]);
          RDCASSERT(argInst);

          op.op->arguments.push_back(argInst);
        }

        {
          SPVInstruction *argInst = module.GetByID(spirv[it + 4]);
          RDCASSERT(argInst);

          op.op->arguments.push_back(argInst);
        }

        for(int i = 5; i < WordCount; i++)
          op.op->literals.push_back(spirv[it + i]);

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpExtInst:
      {
        SPVInstruction *typeInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(typeInst && typeInst->type);

        op.op = new SPVOperation();
        op.op->type = typeInst->type;

        {
          SPVInstruction *setInst = module.GetByID(spirv[it + 3]);
          RDCASSERT(setInst);

          op.op->arguments.push_back(setInst);
        }

        op.op->literals.push_back(spirv[it + 4]);

        for(int i = 5; i < WordCount; i++)
        {
          SPVInstruction *argInst = module.GetByID(spirv[it + i]);
          RDCASSERT(argInst);

          op.op->arguments.push_back(argInst);
        }

        op.id = spirv[it + 2];
        module.ids[spirv[it + 2]] = &op;

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpVectorExtractDynamic:
      case spv::OpArrayLength:
      case spv::OpCompositeExtract:
      case spv::OpCompositeInsert:
      {
        int word = 1;

        SPVInstruction *typeInst = module.GetByID(spirv[it + word]);
        RDCASSERT(typeInst && typeInst->type);

        op.op = new SPVOperation();
        op.op->type = typeInst->type;

        word++;

        op.id = spirv[it + word];
        module.ids[spirv[it + word]] = &op;

        word++;

        SPVInstruction *objInst = NULL;
        if(op.opcode == spv::OpCompositeInsert)
        {
          op.op->complexity = NEVER_INLINE_COMPLEXITY;    // never combine composite insert

          objInst = module.GetByID(spirv[it + word]);
          RDCASSERT(objInst);

          word++;
        }

        SPVInstruction *compInst = module.GetByID(spirv[it + word]);
        RDCASSERT(compInst);

        word++;

        op.op->arguments.push_back(compInst);
        if(objInst)
          op.op->arguments.push_back(objInst);

        if(op.opcode == spv::OpVectorExtractDynamic)
        {
          SPVInstruction *idxInst = module.GetByID(spirv[it + word]);
          RDCASSERT(idxInst);

          op.op->arguments.push_back(idxInst);
          word++;
        }
        else
        {
          for(; word < WordCount; word++)
            op.op->literals.push_back(spirv[it + word]);
        }

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpAtomicStore:
      case spv::OpAtomicExchange:
      case spv::OpAtomicCompareExchange:
      case spv::OpAtomicIIncrement:
      case spv::OpAtomicIDecrement:
      case spv::OpAtomicIAdd:
      case spv::OpAtomicISub:
      case spv::OpAtomicSMin:
      case spv::OpAtomicUMin:
      case spv::OpAtomicSMax:
      case spv::OpAtomicUMax:
      case spv::OpAtomicAnd:
      case spv::OpAtomicOr:
      case spv::OpAtomicXor:
      {
        int word = 1;

        op.op = new SPVOperation();

        // all atomic operations but store return a new ID of a given type
        if(op.opcode != spv::OpAtomicStore)
        {
          SPVInstruction *typeInst = module.GetByID(spirv[it + word]);
          RDCASSERT(typeInst && typeInst->type);
          op.op->type = typeInst->type;

          word++;

          op.id = spirv[it + word];
          module.ids[spirv[it + word]] = &op;

          word++;
        }

        SPVInstruction *ptrInst = module.GetByID(spirv[it + word]);
        RDCASSERT(ptrInst);

        op.op->arguments.push_back(ptrInst);

        word++;

        SPVInstruction *scopeInst = module.GetByID(spirv[it + word]);
        RDCASSERT(scopeInst &&
                  scopeInst->constant);    // shader capability requires this to be a constant

        if(scopeInst && scopeInst->constant)
          op.op->scope = (spv::Scope)scopeInst->constant->u32;

        word++;

        SPVInstruction *semanticsInst = module.GetByID(spirv[it + word]);
        RDCASSERT(semanticsInst &&
                  semanticsInst->constant);    // shader capability requires this to be a constant

        if(semanticsInst && semanticsInst->constant)
          op.op->semantics = (spv::MemorySemanticsMask)semanticsInst->constant->u32;

        word++;

        // compare-exchange operations define an additional semantics for the unequal case
        if(op.opcode == spv::OpAtomicCompareExchange)
        {
          semanticsInst = module.GetByID(spirv[it + word]);
          RDCASSERT(semanticsInst &&
                    semanticsInst->constant);    // shader capability requires this to be a constant

          if(semanticsInst && semanticsInst->constant)
            op.op->semanticsUnequal = (spv::MemorySemanticsMask)semanticsInst->constant->u32;

          word++;
        }

        // all but increment/decrement and load then take a value
        if(op.opcode != spv::OpAtomicIIncrement && op.opcode != spv::OpAtomicIDecrement &&
           op.opcode != spv::OpAtomicLoad)
        {
          SPVInstruction *valueInst = module.GetByID(spirv[it + word]);
          RDCASSERT(valueInst);

          op.op->arguments.push_back(valueInst);

          word++;
        }

        // compare exchange then takes a comparison value
        if(op.opcode == spv::OpAtomicCompareExchange)
        {
          SPVInstruction *compareInst = module.GetByID(spirv[it + word]);
          RDCASSERT(compareInst);

          op.op->arguments.push_back(compareInst);

          word++;
        }

        // never combine atomic operations
        op.op->complexity = NEVER_INLINE_COMPLEXITY;

        curBlock->instructions.push_back(&op);
        break;
      }
      case spv::OpName:
      case spv::OpMemberName:
      case spv::OpLine:
      case spv::OpNoLine:
      case spv::OpDecorate:
      case spv::OpMemberDecorate:
      case spv::OpGroupDecorate:
      case spv::OpGroupMemberDecorate:
      case spv::OpDecorationGroup:
        // Handled in second pass once all IDs are in place
        break;
      case spv::OpNop:
        // nothing to do, ignore
        break;
      default:
      {
        // we should not crash if we don't recognise/handle an opcode - this may happen because of
        // extended SPIR-V or simply custom instructions we don't recognise.
        RDCWARN("Unhandled opcode %s - result ID will be missing", ToStr(op.opcode).c_str());
        if(curBlock)
          curBlock->instructions.push_back(&op);
        break;
      }
    }

    it += WordCount;
  }

  // second pass now that we have all ids set up, apply decorations/names/etc
  it = 5;
  while(it < spirvLength)
  {
    uint16_t WordCount = spirv[it] >> spv::WordCountShift;
    spv::Op op = spv::Op(spirv[it] & spv::OpCodeMask);

    switch(op)
    {
      case spv::OpName:
      {
        SPVInstruction *varInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(varInst);

        varInst->str = (const char *)&spirv[it + 2];

        // strip any 'encoded type' information from function names
        if(varInst->opcode == spv::OpFunction)
        {
          size_t bracket = varInst->str.find('(');
          if(bracket != std::string::npos)
            varInst->str = varInst->str.substr(0, bracket);
        }

        if(varInst->type)
          varInst->type->name = varInst->str;
        break;
      }
      case spv::OpMemberName:
      {
        SPVInstruction *varInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(varInst && varInst->type && varInst->type->type == SPVTypeData::eStruct);
        uint32_t memIdx = spirv[it + 2];
        RDCASSERT(memIdx < varInst->type->children.size());
        varInst->type->children[memIdx].second = (const char *)&spirv[it + 3];
        break;
      }
      case spv::OpLine:
      {
        // VKTODOLOW this now applies to all statements until OpNoLine or end of block
        break;
      }
      case spv::OpNoLine:
      {
        // see above
        break;
      }
      case spv::OpDecorate:
      {
        SPVInstruction *inst = module.GetByID(spirv[it + 1]);
        RDCASSERT(inst);

        SPVDecoration d;
        d.decoration = spv::Decoration(spirv[it + 2]);

        // TODO this isn't enough for all decorations
        RDCASSERT(WordCount <= 4);
        if(WordCount > 3)
          d.val = spirv[it + 3];

        inst->decorations.push_back(d);

        if(inst->type)
          inst->type->decorations = &inst->decorations;

        if(d.decoration == spv::DecorationSpecId && inst->opcode != spv::OpSpecConstantOp)
        {
          if(inst->str.empty())
            inst->str = StringFormat::Fmt("specConstant%u", d.val);

          module.specConstants.push_back(inst);
        }

        break;
      }
      case spv::OpMemberDecorate:
      {
        SPVInstruction *structInst = module.GetByID(spirv[it + 1]);
        RDCASSERT(structInst && structInst->type && structInst->type->type == SPVTypeData::eStruct);

        uint32_t memberIdx = spirv[it + 2];
        RDCASSERT(memberIdx < structInst->type->children.size());

        SPVDecoration d;
        d.decoration = spv::Decoration(spirv[it + 3]);

        // TODO this isn't enough for all decorations
        RDCASSERT(WordCount <= 5);
        if(WordCount > 4)
          d.val = spirv[it + 4];

        structInst->type->childDecorations[memberIdx].push_back(d);
        break;
      }
      case spv::OpGroupDecorate:
      case spv::OpGroupMemberDecorate:
      case spv::OpDecorationGroup: RDCUNIMPLEMENTED("SPIR-V Group decorations"); break;
      default: break;
    }

    it += WordCount;
  }

  struct SortByVarClass
  {
    bool operator()(const SPVInstruction *a, const SPVInstruction *b)
    {
      RDCASSERT(a->var && b->var);

      return a->var->storage < b->var->storage;
    }
  };

  std::sort(module.globals.begin(), module.globals.end(), SortByVarClass());
}
