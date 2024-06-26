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

#include "spirv_common.h"
#include "api/replay/replay_enums.h"
#include "common/common.h"
#include "common/formatting.h"

template <>
rdcstr DoStringise(const rdcspv::Id &el)
{
  uint32_t id;
  RDCCOMPILE_ASSERT(sizeof(el) == sizeof(id), "SPIR-V Id isn't 32-bit!");
  memcpy(&id, &el, sizeof(el));
  return StringFormat::Fmt("%u", id);
}

void rdcspv::Iter::nopRemove(size_t idx, size_t count)
{
  RDCASSERT(idx >= 1);
  size_t oldSize = size();

  if(count == 0)
    count = oldSize - idx;

  // reduce the size of this op
  word(0) = rdcspv::Operation::MakeHeader(opcode(), oldSize - count);

  if(idx + count < oldSize)
  {
    // move any words on the end into the middle, then nop them
    for(size_t i = 0; i < count; i++)
    {
      word(idx + i) = word(idx + count + i);
      word(oldSize - i - 1) = OpNopWord;
    }
  }
  else
  {
    for(size_t i = 0; i < count; i++)
    {
      word(idx + i) = OpNopWord;
    }
  }
}

void rdcspv::Iter::nopRemove()
{
  for(size_t i = 0, sz = size(); i < sz; i++)
    word(i) = OpNopWord;
}

rdcspv::Iter &rdcspv::Iter::operator=(const Operation &op)
{
  size_t newSize = op.size();
  size_t oldSize = size();
  if(newSize > oldSize)
  {
    RDCERR("Can't resize up from %zu to %zu", oldSize, newSize);
    return *this;
  }

  memcpy(&cur(), &op[0], sizeof(uint32_t) * RDCMIN(oldSize, newSize));

  // set remaining words to NOP if we reduced the size
  for(size_t i = newSize; i < oldSize; i++)
    word(i) = OpNopWord;

  return *this;
}

ShaderStage MakeShaderStage(rdcspv::ExecutionModel model)
{
  switch(model)
  {
    case rdcspv::ExecutionModel::Vertex: return ShaderStage::Vertex;
    case rdcspv::ExecutionModel::TessellationControl: return ShaderStage::Tess_Control;
    case rdcspv::ExecutionModel::TessellationEvaluation: return ShaderStage::Tess_Eval;
    case rdcspv::ExecutionModel::Geometry: return ShaderStage::Geometry;
    case rdcspv::ExecutionModel::Fragment: return ShaderStage::Fragment;
    case rdcspv::ExecutionModel::GLCompute: return ShaderStage::Compute;
    case rdcspv::ExecutionModel::TaskEXT: return ShaderStage::Task;
    case rdcspv::ExecutionModel::MeshEXT: return ShaderStage::Mesh;
    case rdcspv::ExecutionModel::RayGenerationKHR: return ShaderStage::RayGen;
    case rdcspv::ExecutionModel::IntersectionKHR: return ShaderStage::Intersection;
    case rdcspv::ExecutionModel::AnyHitKHR: return ShaderStage::AnyHit;
    case rdcspv::ExecutionModel::ClosestHitKHR: return ShaderStage::ClosestHit;
    case rdcspv::ExecutionModel::MissKHR: return ShaderStage::Miss;
    case rdcspv::ExecutionModel::CallableKHR: return ShaderStage::Callable;
    case rdcspv::ExecutionModel::Kernel:
    case rdcspv::ExecutionModel::TaskNV:
    case rdcspv::ExecutionModel::MeshNV:
      // all of these are currently unsupported
      break;
    case rdcspv::ExecutionModel::Invalid:
    case rdcspv::ExecutionModel::Max: break;
  }

  return ShaderStage::Count;
}

ShaderBuiltin MakeShaderBuiltin(ShaderStage stage, const rdcspv::BuiltIn el)
{
  // not complete, might need to expand system attribute list

  switch(el)
  {
    case rdcspv::BuiltIn::Position: return ShaderBuiltin::Position;
    case rdcspv::BuiltIn::PointSize: return ShaderBuiltin::PointSize;
    case rdcspv::BuiltIn::ClipDistance: return ShaderBuiltin::ClipDistance;
    case rdcspv::BuiltIn::CullDistance: return ShaderBuiltin::CullDistance;
    case rdcspv::BuiltIn::VertexId: return ShaderBuiltin::VertexIndex;
    case rdcspv::BuiltIn::InstanceId: return ShaderBuiltin::InstanceIndex;
    case rdcspv::BuiltIn::PrimitiveId: return ShaderBuiltin::PrimitiveIndex;
    case rdcspv::BuiltIn::InvocationId:
    {
      if(stage == ShaderStage::Geometry)
        return ShaderBuiltin::GSInstanceIndex;
      else
        return ShaderBuiltin::OutputControlPointIndex;
    }
    case rdcspv::BuiltIn::Layer: return ShaderBuiltin::RTIndex;
    case rdcspv::BuiltIn::ViewportIndex: return ShaderBuiltin::ViewportIndex;
    case rdcspv::BuiltIn::TessLevelOuter: return ShaderBuiltin::OuterTessFactor;
    case rdcspv::BuiltIn::TessLevelInner: return ShaderBuiltin::InsideTessFactor;
    case rdcspv::BuiltIn::PatchVertices: return ShaderBuiltin::PatchNumVertices;
    case rdcspv::BuiltIn::FragCoord: return ShaderBuiltin::Position;
    case rdcspv::BuiltIn::FrontFacing: return ShaderBuiltin::IsFrontFace;
    case rdcspv::BuiltIn::SampleId: return ShaderBuiltin::MSAASampleIndex;
    case rdcspv::BuiltIn::SamplePosition: return ShaderBuiltin::MSAASamplePosition;
    case rdcspv::BuiltIn::SampleMask: return ShaderBuiltin::MSAACoverage;
    case rdcspv::BuiltIn::FragDepth: return ShaderBuiltin::DepthOutput;
    case rdcspv::BuiltIn::VertexIndex: return ShaderBuiltin::VertexIndex;
    case rdcspv::BuiltIn::InstanceIndex: return ShaderBuiltin::InstanceIndex;
    case rdcspv::BuiltIn::BaseVertex: return ShaderBuiltin::BaseVertex;
    case rdcspv::BuiltIn::BaseInstance: return ShaderBuiltin::BaseInstance;
    case rdcspv::BuiltIn::DrawIndex: return ShaderBuiltin::DrawIndex;
    case rdcspv::BuiltIn::ViewIndex: return ShaderBuiltin::MultiViewIndex;
    case rdcspv::BuiltIn::FragStencilRefEXT: return ShaderBuiltin::StencilReference;
    case rdcspv::BuiltIn::NumWorkgroups: return ShaderBuiltin::DispatchSize;
    case rdcspv::BuiltIn::GlobalInvocationId: return ShaderBuiltin::DispatchThreadIndex;
    case rdcspv::BuiltIn::WorkgroupId: return ShaderBuiltin::GroupIndex;
    case rdcspv::BuiltIn::WorkgroupSize: return ShaderBuiltin::GroupSize;
    case rdcspv::BuiltIn::LocalInvocationIndex: return ShaderBuiltin::GroupFlatIndex;
    case rdcspv::BuiltIn::LocalInvocationId: return ShaderBuiltin::GroupThreadIndex;
    case rdcspv::BuiltIn::TessCoord: return ShaderBuiltin::DomainLocation;
    case rdcspv::BuiltIn::PointCoord: return ShaderBuiltin::PointCoord;
    case rdcspv::BuiltIn::HelperInvocation: return ShaderBuiltin::IsHelper;
    case rdcspv::BuiltIn::SubgroupSize: return ShaderBuiltin::SubgroupSize;
    case rdcspv::BuiltIn::NumSubgroups: return ShaderBuiltin::NumSubgroups;
    case rdcspv::BuiltIn::SubgroupId: return ShaderBuiltin::SubgroupIndexInWorkgroup;
    case rdcspv::BuiltIn::SubgroupLocalInvocationId: return ShaderBuiltin::IndexInSubgroup;
    case rdcspv::BuiltIn::SubgroupEqMask: return ShaderBuiltin::SubgroupEqualMask;
    case rdcspv::BuiltIn::SubgroupGeMask: return ShaderBuiltin::SubgroupGreaterEqualMask;
    case rdcspv::BuiltIn::SubgroupGtMask: return ShaderBuiltin::SubgroupGreaterMask;
    case rdcspv::BuiltIn::SubgroupLeMask: return ShaderBuiltin::SubgroupLessEqualMask;
    case rdcspv::BuiltIn::SubgroupLtMask: return ShaderBuiltin::SubgroupLessMask;
    case rdcspv::BuiltIn::DeviceIndex: return ShaderBuiltin::DeviceIndex;
    case rdcspv::BuiltIn::FullyCoveredEXT: return ShaderBuiltin::IsFullyCovered;
    case rdcspv::BuiltIn::BaryCoordKHR: return ShaderBuiltin::Barycentrics;
    case rdcspv::BuiltIn::FragSizeEXT: return ShaderBuiltin::FragAreaSize;
    case rdcspv::BuiltIn::FragInvocationCountEXT: return ShaderBuiltin::FragInvocationCount;
    case rdcspv::BuiltIn::PrimitivePointIndicesEXT: return ShaderBuiltin::OutputIndices;
    case rdcspv::BuiltIn::PrimitiveLineIndicesEXT: return ShaderBuiltin::OutputIndices;
    case rdcspv::BuiltIn::PrimitiveTriangleIndicesEXT: return ShaderBuiltin::OutputIndices;
    case rdcspv::BuiltIn::CullPrimitiveEXT: return ShaderBuiltin::CullPrimitive;
    case rdcspv::BuiltIn::ShadingRateKHR: return ShaderBuiltin::PackedFragRate;
    default: break;
  }

  RDCWARN("Couldn't map SPIR-V built-in %s to known built-in", ToStr(el).c_str());

  return ShaderBuiltin::Undefined;
}

namespace rdcspv
{

bool ManualForEachID(const ConstIter &it, const std::function<void(Id, bool)> &callback)
{
  switch(it.opcode())
  {
    case rdcspv::Op::Switch:
      // Include just the selector
      callback(Id::fromWord(it.word(1)), false);
      return true;
    default:
      // unhandled
      return false;
  }
}

};    // namespace rdcspv
