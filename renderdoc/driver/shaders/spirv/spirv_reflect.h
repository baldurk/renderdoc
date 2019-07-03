/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#pragma once

#include <vector>
#include "api/replay/renderdoc_replay.h"

struct SPVInstruction;

enum class GraphicsAPI : uint32_t;
enum class ShaderStage : uint32_t;
enum class ShaderBuiltin : uint32_t;
struct ShaderReflection;
struct ShaderBindpointMapping;

// extra information that goes along with a ShaderReflection that has extra information for SPIR-V
// patching
struct SPIRVPatchData
{
  struct InterfaceAccess
  {
    // ID of the base variable
    uint32_t ID = 0;

    // ID of the struct parent of this variable
    uint32_t structID = 0;

    // member in the parent struct of this variable (for MemberDecorate)
    uint32_t structMemberIndex = 0;

    // the access chain of indices
    std::vector<uint32_t> accessChain;

    // is this input/output part of a matrix
    bool isMatrix = false;

    // this is an element of an array that's been exploded after [0].
    // i.e. this is false for non-arrays, and false for element [0] in an array, then true for
    // elements [1], [2], [3], etc..
    bool isArraySubsequentElement = false;
  };

  // matches the input/output signature array, with details of where to fetch the output from in the
  // SPIR-V.
  std::vector<InterfaceAccess> inputs;
  std::vector<InterfaceAccess> outputs;

  // the output topology for tessellation and geometry shaders
  Topology outTopo = Topology::Unknown;
};

#ifndef spirv_HPP
namespace spv
{
using SourceLanguage = uint32_t;
using Capability = uint32_t;
};
#endif

struct SPVModule
{
  SPVModule();
  ~SPVModule();

  std::vector<uint32_t> spirv;

  struct
  {
    uint8_t major, minor;
  } moduleVersion;
  uint32_t generator;

  spv::SourceLanguage sourceLang;
  uint32_t sourceVer;

  std::string cmdline;
  std::vector<rdcpair<std::string, std::string>> sourceFiles;

  std::vector<std::string> extensions;

  std::vector<spv::Capability> capabilities;

  std::vector<SPVInstruction *>
      operations;    // all operations (including those that don't generate an ID)

  std::vector<SPVInstruction *> ids;    // pointers indexed by ID

  std::vector<SPVInstruction *> sourceexts;       // source extensions
  std::vector<SPVInstruction *> entries;          // entry points
  std::vector<SPVInstruction *> globals;          // global variables
  std::vector<SPVInstruction *> specConstants;    // specialization constants
  std::vector<SPVInstruction *> funcs;            // functions
  std::vector<SPVInstruction *> structs;          // struct types

  SPVInstruction *GetByID(uint32_t id);
  std::string Disassemble(const std::string &entryPoint);

  std::vector<std::string> EntryPoints() const;
  ShaderStage StageForEntry(const std::string &entryPoint) const;

  void MakeReflection(GraphicsAPI sourceAPI, ShaderStage stage, const std::string &entryPoint,
                      ShaderReflection &reflection, ShaderBindpointMapping &mapping,
                      SPIRVPatchData &patchData) const;
};

static const uint32_t SpecializationConstantBindSet = 1234567;
static const uint32_t PushConstantBindSet = 1234568;

void ParseSPIRV(uint32_t *spirv, size_t spirvLength, SPVModule &module);

struct SpecConstant
{
  SpecConstant() = default;
  SpecConstant(uint32_t id, uint64_t val, size_t size) : specID(id), value(val), dataSize(size) {}
  uint32_t specID = 0;
  uint64_t value = 0;
  size_t dataSize = 0;
};

void FillSpecConstantVariables(const rdcarray<ShaderConstant> &invars,
                               rdcarray<ShaderVariable> &outvars,
                               const std::vector<SpecConstant> &specInfo);

// common function used by any API that utilises SPIR-V
void AddXFBAnnotations(const ShaderReflection &refl, const SPIRVPatchData &patchData,
                       const char *entryName, std::vector<uint32_t> &modSpirv, uint32_t &xfbStride);

// new reflection interface

#include "spirv_processor.h"

namespace rdcspv
{
struct SourceFile
{
  SourceLanguage lang;
  rdcstr name;
  rdcstr contents;
};

class Reflector : public Processor
{
public:
  Reflector();
  virtual void Parse(const std::vector<uint32_t> &spirvWords);

  std::string Disassemble(const std::string &entryPoint);

  std::vector<std::string> EntryPoints() const;
  ShaderStage StageForEntry(const std::string &entryPoint) const;

  void MakeReflection(const GraphicsAPI sourceAPI, const ShaderStage stage,
                      const std::string &entryPoint, const std::vector<SpecConstant> &specInfo,
                      ShaderReflection &reflection, ShaderBindpointMapping &mapping,
                      SPIRVPatchData &patchData) const;

private:
  virtual void PreParse(uint32_t maxId);
  virtual void PostParse();
  virtual void RegisterOp(Iter iter);
  virtual void UnregisterOp(Iter iter);

  ShaderVariable EvaluateConstant(Id constID, const std::vector<SpecConstant> &specInfo) const;

  void MakeConstantBlockVariables(const DataType &structType, uint32_t arraySize,
                                  uint32_t arrayByteStride, rdcarray<ShaderConstant> &cblock,
                                  const std::vector<SpecConstant> &specInfo) const;
  void MakeConstantBlockVariable(ShaderConstant &outConst, const DataType &type, const rdcstr &name,
                                 const Decorations &decorations,
                                 const std::vector<SpecConstant> &specInfo) const;
  void AddSignatureParameter(const bool isInput, const ShaderStage stage, const Id id,
                             const Id structID, uint32_t &regIndex,
                             const SPIRVPatchData::InterfaceAccess &parentPatch,
                             const rdcstr &varName, const DataType &type,
                             const Decorations &decorations, rdcarray<SigParameter> &sigarray,
                             SPIRVPatchData &patchData,
                             const std::vector<SpecConstant> &specInfo) const;

  rdcstr cmdline;
  DenseIdMap<rdcstr> strings;
  rdcarray<SourceFile> sources;

  struct MemberName
  {
    Id id;
    uint32_t member;
    rdcstr name;
  };

  rdcarray<MemberName> memberNames;
};
};