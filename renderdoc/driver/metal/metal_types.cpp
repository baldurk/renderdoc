/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2023 Baldur Karlsson
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

#include "metal_types.h"
#include "metal_blit_command_encoder.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_command_queue.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_library.h"
#include "metal_manager.h"
#include "metal_render_command_encoder.h"
#include "metal_render_pipeline_state.h"
#include "metal_resources.h"
#include "metal_texture.h"

RDCCOMPILE_ASSERT(sizeof(NS::Integer) == sizeof(std::intptr_t), "NS::Integer size does not match");
RDCCOMPILE_ASSERT(sizeof(NS::UInteger) == sizeof(std::uintptr_t),
                  "NS::UInteger size does not match");

#define DEFINE_OBJC_HELPERS(CPPTYPE)                                                              \
  void AllocateObjCBridge(WrappedMTL##CPPTYPE *wrappedCPP)                                        \
  {                                                                                               \
    RDCCOMPILE_ASSERT((offsetof(WrappedMTL##CPPTYPE, m_ObjcBridge) == 0),                         \
                      "m_ObjcBridge must be at offsetof 0");                                      \
    const char *const className = "ObjCBridgeMTL" #CPPTYPE;                                       \
    static Class klass = objc_lookUpClass(className);                                             \
    static size_t classSize = class_getInstanceSize(klass);                                       \
    if(classSize != sizeof(wrappedCPP->m_ObjcBridge))                                             \
    {                                                                                             \
      RDCFATAL("'%s' classSize != sizeof(m_ObjcBridge) %lu != %lu", className, classSize,         \
               sizeof(wrappedCPP->m_ObjcBridge));                                                 \
    }                                                                                             \
    id objc = objc_constructInstance(klass, &wrappedCPP->m_ObjcBridge);                           \
    if(objc != (id)&wrappedCPP->m_ObjcBridge)                                                     \
    {                                                                                             \
      RDCFATAL("'%s' objc != m_ObjcBridge %p != %p", className, objc, &wrappedCPP->m_ObjcBridge); \
    }                                                                                             \
    MTL::CPPTYPE *real = (MTL::CPPTYPE *)wrappedCPP->m_Real;                                      \
    if(real)                                                                                      \
    {                                                                                             \
      objc_setAssociatedObject((id)real, objc, objc, OBJC_ASSOCIATION_RETAIN);                    \
      ((MTL::CPPTYPE *)objc)->release();                                                          \
    }                                                                                             \
  }                                                                                               \
  void DeallocateObjCBridge(WrappedMTL##CPPTYPE *wrappedCPP)                                      \
  {                                                                                               \
    wrappedCPP->m_ObjcBridge = NULL;                                                              \
    wrappedCPP->m_Real = NULL;                                                                    \
    wrappedCPP->GetResourceManager()->ReleaseWrappedResource(wrappedCPP);                         \
  }

METALCPP_WRAPPED_PROTOCOLS(DEFINE_OBJC_HELPERS)
#undef DEFINE_OBJC_HELPERS

TrackedCAMetalLayer::TrackedCAMetalLayer(CA::MetalLayer *mtlLayer, WrappedMTLDevice *device)
{
  m_mtlLayer = mtlLayer;
  m_Device = device;

  RDCCOMPILE_ASSERT((offsetof(TrackedCAMetalLayer, m_ObjcBridge) == 0),
                    "m_ObjcBridge must be at offsetof 0");
  const char *const className = "ObjCTrackedCAMetalLayer";
  static Class klass = objc_lookUpClass(className);
  static size_t classSize = class_getInstanceSize(klass);
  if(classSize != sizeof(m_ObjcBridge))
  {
    RDCFATAL("'%s' classSize != sizeof(m_ObjcBridge) %lu != %lu", className, classSize,
             sizeof(m_ObjcBridge));
  }
  id objc = objc_constructInstance(klass, &m_ObjcBridge);
  if(objc != (id)&m_ObjcBridge)
  {
    RDCFATAL("'%s' objc != m_ObjcBridge %p != %p", className, objc, &m_ObjcBridge);
  }
  objc_setAssociatedObject((id)m_mtlLayer, objc, objc, OBJC_ASSOCIATION_RETAIN);
  ((NS::Object *)objc)->release();
}

void TrackedCAMetalLayer::StopTracking()
{
  m_Device->UnregisterMetalLayer(m_mtlLayer);
  delete this;
}

namespace RDMTL
{
static bool ValidData(MTL::VertexAttributeDescriptor *attribute)
{
  if(attribute->format() == MTL::VertexFormatInvalid)
    return false;
  return true;
}

static bool ValidData(MTL::VertexBufferLayoutDescriptor *layout)
{
  if(layout->stride() == 0)
    return false;
  return true;
}

static bool ValidData(MTL::PipelineBufferDescriptor *descriptor)
{
  if(descriptor->mutability() == MTL::MutabilityDefault)
    return false;
  return true;
}

static bool ValidData(MTL::RenderPipelineColorAttachmentDescriptor *descriptor)
{
  if(descriptor->pixelFormat() == MTL::PixelFormatInvalid)
    return false;
  return true;
}

static bool ValidData(MTL::RenderPassColorAttachmentDescriptor *descriptor)
{
  MTL::RenderPassAttachmentDescriptor *base = (MTL::RenderPassAttachmentDescriptor *)descriptor;
  if(!base->texture() && !base->resolveTexture())
    return false;
  return true;
}

static bool ValidData(MTL::RenderPassSampleBufferAttachmentDescriptor *descriptor)
{
  if(!descriptor->sampleBuffer())
    return false;
  return true;
}

static bool ValidData(MTL::AttributeDescriptor *descriptor)
{
  if(descriptor->format() == MTL::AttributeFormatInvalid)
    return false;
  return true;
}

static bool ValidData(MTL::BufferLayoutDescriptor *descriptor)
{
  if(descriptor->stride() == 0)
    return false;
  return true;
}

static bool ValidData(MTL::ComputePassSampleBufferAttachmentDescriptor *descriptor)
{
  if(!descriptor->sampleBuffer())
    return false;
  return true;
}

template <typename MTL_TYPE>
static void GetWrappedNSArray(rdcarray<typename UnwrapHelper<MTL_TYPE>::Outer *> &to, NS::Array *from)
{
  size_t count = from->count();
  to.resize(count);
  for(size_t i = 0; i < count; ++i)
  {
    to[i] = GetWrapped((MTL_TYPE)from->object(i));
  }
}

#define GETWRAPPEDNSARRAY(TYPE, NAME) GetWrappedNSArray<MTL::TYPE *>(NAME, objc->NAME())

template <typename MTL_TYPE>
static NS::Array *CreateUnwrappedNSArray(rdcarray<typename UnwrapHelper<MTL_TYPE>::Outer *> &from)
{
  int count = from.count();
  if(count)
  {
    rdcarray<MTL_TYPE> unwrapped;
    unwrapped.resize(count);
    for(int i = 0; i < count; ++i)
    {
      unwrapped[i] = Unwrap(from[i]);
    }
    return NS::Array::array((NS::Object **)(unwrapped.data()), unwrapped.count());
  }
  return NULL;
}

template <typename RDMTL_TYPE, typename MTLARRAY_TYPE, typename MTL_TYPE, int MAX_COUNT>
static void GetObjcArray(rdcarray<RDMTL_TYPE> &to, MTLARRAY_TYPE *from, bool (*validData)(MTL_TYPE *))
{
  for(int i = 0; i < MAX_COUNT; ++i)
  {
    MTL_TYPE *el = from->object(i);
    if(el && validData(el))
    {
      to.resize_for_index(i);
      to[i] = RDMTL_TYPE(el);
    }
  }
}

#define GETOBJCARRAY(TYPE, COUNT, NAME, VALIDDATA_FUNC) \
  GetObjcArray<RDMTL::TYPE, MTL::TYPE##Array, MTL::TYPE, COUNT>(NAME, objc->NAME(), VALIDDATA_FUNC)

template <typename MTLARRAY_TYPE, typename RDMTL_TYPE>
static void CopyToObjcArray(MTLARRAY_TYPE *to, rdcarray<RDMTL_TYPE> &from)
{
  for(int i = 0; i < from.count(); ++i)
  {
    from[i].CopyTo(to->object(i));
  }
}

#define COPYTOOBJCARRAY(TYPE, NAME) \
  CopyToObjcArray<MTL::TYPE##Array, RDMTL::TYPE>(objc->NAME(), NAME)

TextureDescriptor::TextureDescriptor(MTL::TextureDescriptor *objc)
{
  textureType = objc->textureType();
  pixelFormat = objc->pixelFormat();
  width = objc->width();
  height = objc->height();
  depth = objc->depth();
  mipmapLevelCount = objc->mipmapLevelCount();
  sampleCount = objc->sampleCount();
  arrayLength = objc->arrayLength();
  resourceOptions = objc->resourceOptions();
  cpuCacheMode = objc->cpuCacheMode();
  storageMode = objc->storageMode();
  hazardTrackingMode = objc->hazardTrackingMode();
  usage = objc->usage();
  allowGPUOptimizedContents = objc->allowGPUOptimizedContents();
  swizzle = objc->swizzle();
}

TextureDescriptor::operator MTL::TextureDescriptor *()
{
  MTL::TextureDescriptor *objc = MTL::TextureDescriptor::alloc()->init();
  objc->setTextureType(textureType);
  objc->setPixelFormat(pixelFormat);
  objc->setWidth(width);
  objc->setHeight(height);
  objc->setDepth(depth);
  objc->setMipmapLevelCount(mipmapLevelCount);
  objc->setSampleCount(sampleCount);
  objc->setArrayLength(arrayLength);
  objc->setResourceOptions(resourceOptions);
  objc->setCpuCacheMode(cpuCacheMode);
  objc->setStorageMode(storageMode);
  objc->setHazardTrackingMode(hazardTrackingMode);
  objc->setUsage(usage);
  objc->setAllowGPUOptimizedContents(allowGPUOptimizedContents);
  objc->setSwizzle(swizzle);
  return objc;
}

RenderPipelineColorAttachmentDescriptor::RenderPipelineColorAttachmentDescriptor(
    MTL::RenderPipelineColorAttachmentDescriptor *objc)
    : pixelFormat(objc->pixelFormat()),
      blendingEnabled(objc->blendingEnabled()),
      sourceRGBBlendFactor(objc->sourceAlphaBlendFactor()),
      destinationRGBBlendFactor(objc->destinationRGBBlendFactor()),
      rgbBlendOperation(objc->rgbBlendOperation()),
      sourceAlphaBlendFactor(objc->sourceAlphaBlendFactor()),
      destinationAlphaBlendFactor(objc->destinationAlphaBlendFactor()),
      alphaBlendOperation(objc->alphaBlendOperation()),
      writeMask(objc->writeMask())
{
}

void RenderPipelineColorAttachmentDescriptor::CopyTo(MTL::RenderPipelineColorAttachmentDescriptor *objc)
{
  objc->setPixelFormat(pixelFormat);
  objc->setBlendingEnabled(blendingEnabled);
  objc->setSourceRGBBlendFactor(sourceRGBBlendFactor);
  objc->setDestinationRGBBlendFactor(destinationRGBBlendFactor);
  objc->setRgbBlendOperation(rgbBlendOperation);
  objc->setSourceAlphaBlendFactor(sourceAlphaBlendFactor);
  objc->setDestinationAlphaBlendFactor(destinationAlphaBlendFactor);
  objc->setAlphaBlendOperation(alphaBlendOperation);
  objc->setWriteMask(writeMask);
}

PipelineBufferDescriptor::PipelineBufferDescriptor(MTL::PipelineBufferDescriptor *objc)
    : mutability(objc->mutability())
{
}

void PipelineBufferDescriptor::CopyTo(MTL::PipelineBufferDescriptor *objc)
{
  objc->setMutability(mutability);
}

VertexAttributeDescriptor::VertexAttributeDescriptor(MTL::VertexAttributeDescriptor *objc)
    : format(objc->format()), offset(objc->offset()), bufferIndex(objc->bufferIndex())
{
}

void VertexAttributeDescriptor::CopyTo(MTL::VertexAttributeDescriptor *objc)
{
  objc->setFormat(format);
  objc->setOffset(offset);
  objc->setBufferIndex(bufferIndex);
}

VertexBufferLayoutDescriptor::VertexBufferLayoutDescriptor(MTL::VertexBufferLayoutDescriptor *objc)
    : stride(objc->stride()), stepFunction(objc->stepFunction()), stepRate(objc->stepRate())
{
}

void VertexBufferLayoutDescriptor::CopyTo(MTL::VertexBufferLayoutDescriptor *objc)
{
  objc->setStride(stride);
  objc->setStepFunction(stepFunction);
  objc->setStepRate(stepRate);
}

VertexDescriptor::VertexDescriptor(MTL::VertexDescriptor *objc)
{
  GETOBJCARRAY(VertexBufferLayoutDescriptor, MAX_VERTEX_SHADER_ATTRIBUTES, layouts, ValidData);
  GETOBJCARRAY(VertexAttributeDescriptor, MAX_VERTEX_SHADER_ATTRIBUTES, attributes, ValidData);
}

void VertexDescriptor::CopyTo(MTL::VertexDescriptor *objc)
{
  COPYTOOBJCARRAY(VertexBufferLayoutDescriptor, layouts);
  COPYTOOBJCARRAY(VertexAttributeDescriptor, attributes);
}

AttributeDescriptor::AttributeDescriptor(MTL::AttributeDescriptor *objc)
    : bufferIndex(objc->bufferIndex()), offset(objc->offset()), format(objc->format())
{
}

void AttributeDescriptor::CopyTo(MTL::AttributeDescriptor *objc)
{
  objc->setBufferIndex(bufferIndex);
  objc->setOffset(offset);
  objc->setFormat(format);
}

BufferLayoutDescriptor::BufferLayoutDescriptor(MTL::BufferLayoutDescriptor *objc)
    : stride(objc->stride()), stepFunction(objc->stepFunction()), stepRate(objc->stepRate())
{
}

void BufferLayoutDescriptor::CopyTo(MTL::BufferLayoutDescriptor *objc)
{
  objc->setStride(stride);
  objc->setStepFunction(stepFunction);
  objc->setStepRate(stepRate);
}

StageInputOutputDescriptor::StageInputOutputDescriptor(MTL::StageInputOutputDescriptor *objc)
    : indexBufferIndex(objc->indexBufferIndex()), indexType(objc->indexType())
{
  GETOBJCARRAY(AttributeDescriptor, MAX_COMPUTE_PASS_BUFFER_ATTACHMENTS, attributes, ValidData);
  GETOBJCARRAY(BufferLayoutDescriptor, MAX_COMPUTE_PASS_BUFFER_ATTACHMENTS, layouts, ValidData);
}

void StageInputOutputDescriptor::CopyTo(MTL::StageInputOutputDescriptor *objc)
{
  COPYTOOBJCARRAY(AttributeDescriptor, attributes);
  COPYTOOBJCARRAY(BufferLayoutDescriptor, layouts);
  objc->setIndexBufferIndex(indexBufferIndex);
  objc->setIndexType(indexType);
}

LinkedFunctions::LinkedFunctions(MTL::LinkedFunctions *objc)
{
  GETWRAPPEDNSARRAY(Function, functions);
  GETWRAPPEDNSARRAY(Function, binaryFunctions);
  {
    NS::Dictionary *objcGroups = objc->groups();
    NS::Array *keys = objcGroups->keyEnumerator()->allObjects();
    size_t countKeys = keys->count();

    groups.resize(countKeys);
    for(size_t i = 0; i < countKeys; ++i)
    {
      NS::String *key = (NS::String *)keys->object(i);
      NS::Array *funcs = (NS::Array *)objcGroups->object(key);
      size_t countFuncs = funcs->count();

      FunctionGroup &funcGroup = groups[i];
      funcGroup.callsite.assign(key->utf8String());
      funcGroup.functions.resize(countFuncs);
      for(size_t j = 0; j < countFuncs; ++j)
      {
        funcGroup.functions[j] = GetWrapped((MTL::Function *)funcs->object(j));
      }
    }
  }
  GETWRAPPEDNSARRAY(Function, privateFunctions);
}

void LinkedFunctions::CopyTo(MTL::LinkedFunctions *objc)
{
  objc->setFunctions(CreateUnwrappedNSArray<MTL::Function *>(functions));
  objc->setBinaryFunctions(CreateUnwrappedNSArray<MTL::Function *>(binaryFunctions));
  {
    NS::Dictionary *inGroups = NULL;
    int countKeys = groups.count();
    if(countKeys)
    {
      rdcarray<NS::Array *> values;
      rdcarray<NS::String *> keys;
      keys.resize(countKeys);
      values.resize(countKeys);
      for(int i = 0; i < countKeys; ++i)
      {
        FunctionGroup &funcGroup = groups[i];
        keys[i] = NS::String::string(funcGroup.callsite.data(), NS::UTF8StringEncoding);
        values[i] = CreateUnwrappedNSArray<MTL::Function *>(funcGroup.functions);
      }

      inGroups = NS::Dictionary::dictionary((NS::Object **)values.data(),
                                            (NS::Object **)keys.data(), countKeys);
    }
    objc->setGroups(inGroups);
  }
  objc->setPrivateFunctions(CreateUnwrappedNSArray<MTL::Function *>(privateFunctions));
}

RenderPipelineDescriptor::RenderPipelineDescriptor(MTL::RenderPipelineDescriptor *objc)
    : vertexFunction(GetWrapped(objc->vertexFunction())),
      fragmentFunction(GetWrapped(objc->fragmentFunction())),
      vertexDescriptor(objc->vertexDescriptor()),
      sampleCount(objc->sampleCount()),
      rasterSampleCount(objc->rasterSampleCount()),
      alphaToCoverageEnabled(objc->alphaToCoverageEnabled()),
      alphaToOneEnabled(objc->alphaToOneEnabled()),
      rasterizationEnabled(objc->rasterizationEnabled()),
      maxVertexAmplificationCount(objc->maxVertexAmplificationCount()),
      depthAttachmentPixelFormat(objc->depthAttachmentPixelFormat()),
      stencilAttachmentPixelFormat(objc->stencilAttachmentPixelFormat()),
      inputPrimitiveTopology(objc->inputPrimitiveTopology()),
      tessellationPartitionMode(objc->tessellationPartitionMode()),
      maxTessellationFactor(objc->maxTessellationFactor()),
      tessellationFactorScaleEnabled(objc->tessellationFactorScaleEnabled()),
      tessellationFactorFormat(objc->tessellationFactorFormat()),
      tessellationControlPointIndexType(objc->tessellationControlPointIndexType()),
      tessellationFactorStepFunction(objc->tessellationFactorStepFunction()),
      tessellationOutputWindingOrder(objc->tessellationOutputWindingOrder()),
      supportIndirectCommandBuffers(objc->supportIndirectCommandBuffers()),
      vertexLinkedFunctions(objc->vertexLinkedFunctions()),
      fragmentLinkedFunctions(objc->fragmentLinkedFunctions()),
      supportAddingVertexBinaryFunctions(objc->supportAddingVertexBinaryFunctions()),
      supportAddingFragmentBinaryFunctions(objc->supportAddingFragmentBinaryFunctions()),
      maxVertexCallStackDepth(objc->maxVertexCallStackDepth()),
      maxFragmentCallStackDepth(objc->maxFragmentCallStackDepth())
{
  if(objc->label())
    label.assign(objc->label()->utf8String());
  GETOBJCARRAY(RenderPipelineColorAttachmentDescriptor, MAX_RENDER_PASS_COLOR_ATTACHMENTS,
               colorAttachments, ValidData);
  GETOBJCARRAY(PipelineBufferDescriptor, MAX_RENDER_PASS_BUFFER_ATTACHMENTS, vertexBuffers,
               ValidData);
  GETOBJCARRAY(PipelineBufferDescriptor, MAX_RENDER_PASS_BUFFER_ATTACHMENTS, fragmentBuffers,
               ValidData);
  // TODO: when WrappedMTLBinaryArchive exists
  // GETWRAPPEDNSARRAY(BinaryArchive, binaryArchives);
  // TODO: when WrappedMTLDynamicLibrary exists
  // GETWRAPPEDNSARRAY(DynamicLibrary, vertexPreloadedLibraries);
  // GETWRAPPEDNSARRAY(DynamicLibrary, fragmentPreloadedLibraries);
}

RenderPipelineDescriptor::operator MTL::RenderPipelineDescriptor *()
{
  MTL::RenderPipelineDescriptor *objc = MTL::RenderPipelineDescriptor::alloc()->init();
  if(label.length() > 0)
  {
    objc->setLabel(NS::String::string(label.data(), NS::UTF8StringEncoding));
  }
  objc->setVertexFunction(Unwrap(vertexFunction));
  objc->setFragmentFunction(Unwrap(fragmentFunction));
  vertexDescriptor.CopyTo(objc->vertexDescriptor());
  objc->setSampleCount(sampleCount);
  objc->setRasterSampleCount(rasterSampleCount);
  objc->setAlphaToCoverageEnabled(alphaToCoverageEnabled);
  objc->setAlphaToOneEnabled(alphaToOneEnabled);
  objc->setRasterizationEnabled(rasterizationEnabled);
  objc->setMaxVertexAmplificationCount(maxVertexAmplificationCount);
  COPYTOOBJCARRAY(RenderPipelineColorAttachmentDescriptor, colorAttachments);
  objc->setDepthAttachmentPixelFormat(depthAttachmentPixelFormat);
  objc->setStencilAttachmentPixelFormat(stencilAttachmentPixelFormat);
  objc->setInputPrimitiveTopology(inputPrimitiveTopology);
  objc->setTessellationPartitionMode(tessellationPartitionMode);
  objc->setMaxTessellationFactor(maxTessellationFactor);
  objc->setTessellationFactorScaleEnabled(tessellationFactorScaleEnabled);
  objc->setTessellationFactorFormat(tessellationFactorFormat);
  objc->setTessellationControlPointIndexType(tessellationControlPointIndexType);
  objc->setTessellationFactorStepFunction(tessellationFactorStepFunction);
  objc->setTessellationOutputWindingOrder(tessellationOutputWindingOrder);
  COPYTOOBJCARRAY(PipelineBufferDescriptor, vertexBuffers);
  COPYTOOBJCARRAY(PipelineBufferDescriptor, fragmentBuffers);
  objc->setSupportIndirectCommandBuffers(supportIndirectCommandBuffers);
  // TODO: when WrappedMTLBinaryArchive exists
  // objc->setBinaryArchives(CreateUnwrappedNSArray<MTL::BinaryArchive *>(binaryArchives));
  // TODO: when WrappedMTLDynamicLibrary exists
  // objc->setVertexPreloadedLibraries(CreateUnwrappedNSArray<MTL::DynamicLibrary
  // *>(vertexPreloadedLibraries));
  // objc->setFragmentPreloadedLibraries(CreateUnwrappedNSArray<MTL::DynamicLibrary
  // *>(fragmentPreloadedLibraries));
  vertexLinkedFunctions.CopyTo(objc->vertexLinkedFunctions());
  fragmentLinkedFunctions.CopyTo(objc->fragmentLinkedFunctions());
  objc->setSupportAddingVertexBinaryFunctions(supportAddingVertexBinaryFunctions);
  objc->setSupportAddingFragmentBinaryFunctions(supportAddingFragmentBinaryFunctions);
  objc->setMaxVertexCallStackDepth(maxVertexCallStackDepth);
  objc->setMaxFragmentCallStackDepth(maxFragmentCallStackDepth);

  return objc;
}

RenderPassAttachmentDescriptor::RenderPassAttachmentDescriptor(MTL::RenderPassAttachmentDescriptor *objc)
    : texture(GetWrapped(objc->texture())),
      level(objc->level()),
      slice(objc->slice()),
      depthPlane(objc->depthPlane()),
      resolveTexture(GetWrapped(objc->resolveTexture())),
      resolveLevel(objc->resolveLevel()),
      resolveSlice(objc->resolveSlice()),
      resolveDepthPlane(objc->resolveDepthPlane()),
      loadAction(objc->loadAction()),
      storeAction(objc->storeAction()),
      storeActionOptions(objc->storeActionOptions())
{
}

void RenderPassAttachmentDescriptor::CopyTo(MTL::RenderPassAttachmentDescriptor *objc)
{
  objc->setTexture(Unwrap(texture));
  objc->setLevel(level);
  objc->setSlice(slice);
  objc->setDepthPlane(depthPlane);
  objc->setResolveTexture(Unwrap(resolveTexture));
  objc->setResolveLevel(resolveLevel);
  objc->setResolveSlice(resolveSlice);
  objc->setResolveDepthPlane(resolveDepthPlane);
  objc->setLoadAction(loadAction);
  objc->setStoreAction(storeAction);
  objc->setStoreActionOptions(storeActionOptions);
}

RenderPassColorAttachmentDescriptor::RenderPassColorAttachmentDescriptor(
    MTL::RenderPassColorAttachmentDescriptor *objc)
    : RenderPassAttachmentDescriptor((MTL::RenderPassAttachmentDescriptor *)objc),
      clearColor(objc->clearColor())
{
}

void RenderPassColorAttachmentDescriptor::CopyTo(MTL::RenderPassColorAttachmentDescriptor *objc)
{
  ((RenderPassAttachmentDescriptor *)this)->CopyTo((MTL::RenderPassAttachmentDescriptor *)objc);
  objc->setClearColor(clearColor);
}

RenderPassDepthAttachmentDescriptor::RenderPassDepthAttachmentDescriptor(
    MTL::RenderPassDepthAttachmentDescriptor *objc)
    : RenderPassAttachmentDescriptor((MTL::RenderPassAttachmentDescriptor *)objc),
      clearDepth(objc->clearDepth()),
      depthResolveFilter(objc->depthResolveFilter())
{
}

void RenderPassDepthAttachmentDescriptor::CopyTo(MTL::RenderPassDepthAttachmentDescriptor *objc)
{
  ((RenderPassAttachmentDescriptor *)this)->CopyTo((MTL::RenderPassAttachmentDescriptor *)objc);
  objc->setClearDepth(clearDepth);
  objc->setDepthResolveFilter(depthResolveFilter);
}

RenderPassStencilAttachmentDescriptor::RenderPassStencilAttachmentDescriptor(
    MTL::RenderPassStencilAttachmentDescriptor *objc)
    : RenderPassAttachmentDescriptor((MTL::RenderPassAttachmentDescriptor *)objc),
      clearStencil(objc->clearStencil()),
      stencilResolveFilter(objc->stencilResolveFilter())
{
}

void RenderPassStencilAttachmentDescriptor::CopyTo(MTL::RenderPassStencilAttachmentDescriptor *objc)
{
  ((RenderPassAttachmentDescriptor *)this)->CopyTo((MTL::RenderPassAttachmentDescriptor *)objc);
  objc->setClearStencil(clearStencil);
  objc->setStencilResolveFilter(stencilResolveFilter);
}

RenderPassSampleBufferAttachmentDescriptor::RenderPassSampleBufferAttachmentDescriptor(
    MTL::RenderPassSampleBufferAttachmentDescriptor *objc)
    :    // TODO: when WrappedMTLCounterSampleBuffer exists
         // sampleBuffer(GetWrapped(objc->sampleBuffer())),
      startOfVertexSampleIndex(objc->startOfVertexSampleIndex()),
      endOfVertexSampleIndex(objc->endOfVertexSampleIndex()),
      startOfFragmentSampleIndex(objc->startOfFragmentSampleIndex()),
      endOfFragmentSampleIndex(objc->endOfFragmentSampleIndex())
{
}

void RenderPassSampleBufferAttachmentDescriptor::CopyTo(
    MTL::RenderPassSampleBufferAttachmentDescriptor *objc)
{
  // TODO: when WrappedMTLCounterSampleBuffer exists
  // objc->setSampleBuffer(Unwrap(sampleBuffer));
  objc->setStartOfVertexSampleIndex(startOfVertexSampleIndex);
  objc->setEndOfVertexSampleIndex(endOfVertexSampleIndex);
  objc->setStartOfFragmentSampleIndex(startOfFragmentSampleIndex);
  objc->setEndOfFragmentSampleIndex(endOfFragmentSampleIndex);
}

RenderPassDescriptor::RenderPassDescriptor(MTL::RenderPassDescriptor *objc)
    : depthAttachment(objc->depthAttachment()),
      stencilAttachment(objc->stencilAttachment()),
      visibilityResultBuffer(GetWrapped(objc->visibilityResultBuffer())),
      renderTargetArrayLength(objc->renderTargetArrayLength()),
      imageblockSampleLength(objc->imageblockSampleLength()),
      threadgroupMemoryLength(objc->threadgroupMemoryLength()),
      tileWidth(objc->tileWidth()),
      tileHeight(objc->tileHeight()),
      defaultRasterSampleCount(objc->defaultRasterSampleCount()),
      renderTargetWidth(objc->renderTargetWidth()),
      renderTargetHeight(objc->renderTargetHeight())
// TODO: when WrappedRasterizationRateMap exists
// rasterizationRateMap(objc->rasterizationRateMap())
{
  GETOBJCARRAY(RenderPassColorAttachmentDescriptor, MAX_RENDER_PASS_COLOR_ATTACHMENTS,
               colorAttachments, ValidData);
  size_t count = objc->getSamplePositions(NULL, 0);
  if(count)
  {
    samplePositions.resize(count);
    objc->getSamplePositions(samplePositions.data(), count);
  }
  GETOBJCARRAY(RenderPassSampleBufferAttachmentDescriptor,
               MAX_RENDER_PASS_SAMPLE_BUFFER_ATTACHMENTS, sampleBufferAttachments, ValidData);
}

RenderPassDescriptor::operator MTL::RenderPassDescriptor *()
{
  MTL::RenderPassDescriptor *objc = MTL::RenderPassDescriptor::alloc()->init();
  COPYTOOBJCARRAY(RenderPassColorAttachmentDescriptor, colorAttachments);
  depthAttachment.CopyTo(objc->depthAttachment());
  stencilAttachment.CopyTo(objc->stencilAttachment());
  objc->setVisibilityResultBuffer(Unwrap(visibilityResultBuffer));
  objc->setRenderTargetArrayLength(renderTargetArrayLength);
  objc->setImageblockSampleLength(imageblockSampleLength);
  objc->setThreadgroupMemoryLength(threadgroupMemoryLength);
  objc->setTileWidth(tileWidth);
  objc->setTileHeight(tileHeight);
  objc->setDefaultRasterSampleCount(defaultRasterSampleCount);
  objc->setRenderTargetWidth(renderTargetWidth);
  objc->setRenderTargetHeight(renderTargetHeight);
  objc->setSamplePositions(samplePositions.data(), samplePositions.count());
  // TODO: when WrappedRasterizationRateMap exists
  // objc->setRasterizationRateMap(Unwrap(rasterizationRateMap));
  COPYTOOBJCARRAY(RenderPassSampleBufferAttachmentDescriptor, sampleBufferAttachments);
  return objc;
}

ComputePassSampleBufferAttachmentDescriptor::ComputePassSampleBufferAttachmentDescriptor(
    MTL::ComputePassSampleBufferAttachmentDescriptor *objc)
    : startOfEncoderSampleIndex(objc->startOfEncoderSampleIndex()),
      endOfEncoderSampleIndex(objc->endOfEncoderSampleIndex())
{
}

void ComputePassSampleBufferAttachmentDescriptor::CopyTo(
    MTL::ComputePassSampleBufferAttachmentDescriptor *objc)
{
  // TODO: when WrappedMTLCounterSampleBuffer exists
  // objc->setSampleBuffer(Unwrap(sampleBuffer));
  objc->setStartOfEncoderSampleIndex(startOfEncoderSampleIndex);
  objc->setEndOfEncoderSampleIndex(endOfEncoderSampleIndex);
}

ComputePipelineDescriptor::ComputePipelineDescriptor(MTL::ComputePipelineDescriptor *objc)
    : computeFunction(GetWrapped(objc->computeFunction())),
      threadGroupSizeIsMultipleOfThreadExecution(
          objc->threadGroupSizeIsMultipleOfThreadExecutionWidth()),
      maxTotalThreadsPerThreadgroup(objc->maxTotalThreadsPerThreadgroup()),
      maxCallStackDepth(objc->maxCallStackDepth()),
      stageInputDescriptor(objc->stageInputDescriptor()),
      supportIndirectCommandBuffers(objc->supportIndirectCommandBuffers()),
      linkedFunctions(objc->linkedFunctions()),
      supportAddingBinaryFunctions(objc->supportAddingBinaryFunctions())
{
  if(objc->label())
    label.assign(objc->label()->utf8String());
  GETOBJCARRAY(PipelineBufferDescriptor, MAX_COMPUTE_PASS_BUFFER_ATTACHMENTS, buffers, ValidData);
  // TODO: when WrappedMTLDynamicLibrary exists
  // GETWRAPPEDNSARRAY(DynamicLibrary, preloadedLibraries)
  // Deprecated
  // GETWRAPPEDNSARRAY(DynamicLibrary, insertLibraries)
  // GETWRAPPEDNSARRAY(DynamicLibrary, linkedFunctions)
  // TODO: when WrappedMTLBinaryArchive exists
  // GETWRAPPEDNSARRAY(BinaryArchive, binaryArchives);
}

ComputePipelineDescriptor::operator MTL::ComputePipelineDescriptor *()
{
  MTL::ComputePipelineDescriptor *objc = MTL::ComputePipelineDescriptor::alloc()->init();
  if(label.length() > 0)
  {
    objc->setLabel(NS::String::string(label.data(), NS::UTF8StringEncoding));
  }
  objc->setComputeFunction(Unwrap(computeFunction));
  objc->setThreadGroupSizeIsMultipleOfThreadExecutionWidth(threadGroupSizeIsMultipleOfThreadExecution);
  objc->setMaxTotalThreadsPerThreadgroup(maxTotalThreadsPerThreadgroup);
  objc->setMaxCallStackDepth(maxCallStackDepth);
  stageInputDescriptor.CopyTo(objc->stageInputDescriptor());
  objc->setSupportIndirectCommandBuffers(supportIndirectCommandBuffers);
  linkedFunctions.CopyTo(objc->linkedFunctions());
  objc->setSupportAddingBinaryFunctions(supportAddingBinaryFunctions);
  COPYTOOBJCARRAY(PipelineBufferDescriptor, buffers);
  // TODO: when WrappedMTLDynamicLibrary exists
  // objc->setPreloadedLibraries(CreateUnwrappedNSArray<MTL::DynamicLibrary *>(preloadedLibraries));
  // Deprecated
  // objc->setInsertLibraries(CreateUnwrappedNSArray<MTL::DynamicLibrary *>(insertLibraries));
  // objc->setLinkedFunctions(CreateUnwrappedNSArray<MTL::DynamicLibrary *>(linkedFunctions));
  // TODO: when WrappedMTLBinaryArchive exists
  // objc->setBinaryArchives(CreateUnwrappedNSArray<MTL::BinaryArchive *>(binaryArchives));
  // GETWRAPPEDNSARRAY(BinaryArchive, binaryArchives);
  return objc;
}

ComputePassDescriptor::ComputePassDescriptor(MTL::ComputePassDescriptor *objc)
    : dispatchType(objc->dispatchType())
{
  GETOBJCARRAY(ComputePassSampleBufferAttachmentDescriptor,
               MAX_COMPUTE_PASS_SAMPLE_BUFFER_ATTACHMENTS, sampleBufferAttachments, ValidData);
}

ComputePassDescriptor::operator MTL::ComputePassDescriptor *()
{
  MTL::ComputePassDescriptor *objc = MTL::ComputePassDescriptor::alloc()->init();
  COPYTOOBJCARRAY(ComputePassSampleBufferAttachmentDescriptor, sampleBufferAttachments);
  objc->setDispatchType(dispatchType);
  return objc;
}

}    // namespace RDMTL
