/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#include "metal_core.h"
#include "serialise/rdcfile.h"
#include "metal_command_buffer.h"
#include "metal_device.h"

WriteSerialiser &WrappedMTLDevice::GetThreadSerialiser()
{
  WriteSerialiser *ser = (WriteSerialiser *)Threading::GetTLSValue(threadSerialiserTLSSlot);
  if(ser)
    return *ser;

  // slow path, but rare
  ser = new WriteSerialiser(new StreamWriter(1024), Ownership::Stream);

  uint32_t flags = WriteSerialiser::ChunkDuration | WriteSerialiser::ChunkTimestamp |
                   WriteSerialiser::ChunkThreadID;

  if(RenderDoc::Inst().GetCaptureOptions().captureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  ser->SetChunkMetadataRecording(flags);
  ser->SetUserData(GetResourceManager());
  ser->SetVersion(MetalInitParams::CurrentVersion);

  Threading::SetTLSValue(threadSerialiserTLSSlot, (void *)ser);

  {
    SCOPED_LOCK(m_ThreadSerialisersLock);
    m_ThreadSerialisers.push_back(ser);
  }

  return *ser;
}

void WrappedMTLDevice::WaitForGPU()
{
  MTL::CommandBuffer *mtlCommandBuffer = m_mtlCommandQueue->commandBuffer();
  mtlCommandBuffer->commit();
  mtlCommandBuffer->waitUntilCompleted();
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_BeginCaptureFrame(SerialiserType &ser)
{
  // TODO: serialise image references and states

  SERIALISE_CHECK_READ_ERRORS();

  return true;
}

void WrappedMTLDevice::StartFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsBackgroundCapturing(m_State))
    return;

  RDCLOG("Starting capture");
  {
    SCOPED_LOCK(m_CaptureCommandBuffersLock);
    RDCASSERT(m_CaptureCommandBuffersSubmitted.empty());
  }

  m_CaptureTimer.Restart();

  GetResourceManager()->ResetCaptureStartTime();

  m_AppControlledCapture = true;

  FrameDescription frame;
  frame.frameNumber = ~0U;
  frame.captureTime = Timing::GetUnixTimestamp();
  m_CapturedFrames.push_back(frame);

  GetResourceManager()->ClearReferencedResources();
  // TODO: handle tracked memory

  // need to do all this atomically so that no other commands
  // will check to see if they need to mark dirty or
  // mark pending dirty and go into the frame record.
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);

    GetResourceManager()->PrepareInitialContents();

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();
    m_State = CaptureState::ActiveCapturing;
  }

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(this), eFrameRef_Read);

  // TODO: are there other resources that need to be marked as frame referenced
}

void WrappedMTLDevice::EndCaptureFrame()
{
  CACHE_THREAD_SERIALISER();
  ser.SetActionChunk();
  SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureEnd);

  // TODO: serialise the presented image

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

bool WrappedMTLDevice::EndFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  // TODO: find the window and drawable being captured

  RDCLOG("Finished capture, Frame %u", m_CapturedFrames.back().frameNumber);

  // TODO: mark the drawable and its images as frame referenced

  // atomically transition to IDLE
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);
    EndCaptureFrame();
    m_State = CaptureState::BackgroundCapturing;
  }

  {
    SCOPED_LOCK(m_CaptureCommandBuffersLock);
    // wait for the GPU to be idle
    for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
    {
      WrappedMTLCommandBuffer *commandBuffer = (WrappedMTLCommandBuffer *)(record->m_Resource);
      Unwrap(commandBuffer)->waitUntilCompleted();
      // Remove the reference on the real resource added during commit()
      Unwrap(commandBuffer)->release();
    }

    if(m_CaptureCommandBuffersSubmitted.empty())
      WaitForGPU();
  }

  // TODO: get the backbuffer to generate the thumbnail image
  RenderDoc::FramePixels fp;

  RDCFile *rdc =
      RenderDoc::Inst().CreateRDC(RDCDriver::Metal, m_CapturedFrames.back().frameNumber, fp);

  StreamWriter *captureWriter = NULL;

  if(rdc)
  {
    SectionProperties props;

    // Compress with LZ4 so that it's fast
    props.flags = SectionFlags::LZ4Compressed;
    props.version = m_SectionVersion;
    props.type = SectionType::FrameCapture;

    captureWriter = rdc->WriteSection(props);
  }
  else
  {
    captureWriter = new StreamWriter(StreamWriter::InvalidStream);
  }

  uint64_t captureSectionSize = 0;

  {
    WriteSerialiser ser(captureWriter, Ownership::Stream);

    ser.SetChunkMetadataRecording(GetThreadSerialiser().GetChunkMetadataRecording());
    ser.SetUserData(GetResourceManager());

    {
      m_InitParams.Set(Unwrap(this), m_ID);
      SCOPED_SERIALISE_CHUNK(SystemChunk::DriverInit, m_InitParams.GetSerialiseSize());
      SERIALISE_ELEMENT(m_InitParams);
    }

    RDCDEBUG("Inserting Resource Serialisers");
    GetResourceManager()->InsertReferencedChunks(ser);
    GetResourceManager()->InsertInitialContentsChunks(ser);

    RDCDEBUG("Creating Capture Scope");
    GetResourceManager()->Serialise_InitialContentsNeeded(ser);
    // TODO: memory references

    // need over estimate of chunk size when writing directly to file
    {
      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureScope, 16);
      Serialise_CaptureScope(ser);
    }

    {
      uint64_t maxCaptureBeginChunkSizeInBytes = 16;
      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureBegin, maxCaptureBeginChunkSizeInBytes);
      Serialise_BeginCaptureFrame(ser);
    }

    // don't need to lock access to m_CaptureCommandBuffersSubmitted as
    // no longer in active capture (the transition is thread-protected)
    // nothing will be pushed to the vector

    {
      std::map<int64_t, Chunk *> recordlist;
      size_t countCmdBuffers = m_CaptureCommandBuffersSubmitted.size();
      // ensure all command buffer records within the frame even if recorded before
      // serialised order must be preserved
      for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
      {
        size_t prevSize = recordlist.size();
        (void)prevSize;
        record->Insert(recordlist);
      }

      size_t prevSize = recordlist.size();
      (void)prevSize;
      m_FrameCaptureRecord->Insert(recordlist);
      RDCDEBUG("Adding %zu/%zu frame capture chunks to file serialiser",
               recordlist.size() - prevSize, recordlist.size());

      float num = float(recordlist.size());
      float idx = 0.0f;

      for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
      {
        RenderDoc::Inst().SetProgress(CaptureProgress::SerialiseFrameContents, idx / num);
        idx += 1.0f;
        it->second->Write(ser);
      }
    }
    captureSectionSize = captureWriter->GetOffset();
  }

  RDCLOG("Captured Metal frame with %f MB capture section in %f seconds",
         double(captureSectionSize) / (1024.0 * 1024.0), m_CaptureTimer.GetMilliseconds() / 1000.0);

  RenderDoc::Inst().FinishCaptureWriting(rdc, m_CapturedFrames.back().frameNumber);

  // delete tracked cmd buffers - had to keep them alive until after serialiser flush.
  CaptureClearSubmittedCmdBuffers();

  GetResourceManager()->ResetLastWriteTimes();
  GetResourceManager()->MarkUnwrittenResources();

  // TODO: handle memory resources in the resource manager

  GetResourceManager()->ClearReferencedResources();
  GetResourceManager()->FreeInitialContents();

  // TODO: handle memory resources in the initial contents

  return true;
}

bool WrappedMTLDevice::DiscardFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  RDCLOG("Discarding frame capture.");

  RenderDoc::Inst().FinishCaptureWriting(NULL, m_CapturedFrames.back().frameNumber);

  m_CapturedFrames.pop_back();

  // atomically transition to IDLE
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);
    m_State = CaptureState::BackgroundCapturing;
  }

  CaptureClearSubmittedCmdBuffers();

  GetResourceManager()->MarkUnwrittenResources();

  // TODO: handle memory resources in the resource manager

  GetResourceManager()->ClearReferencedResources();
  GetResourceManager()->FreeInitialContents();

  // TODO: handle memory resources in the initial contents

  return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_CaptureScope(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(frameNumber, m_CapturedFrames.back().frameNumber);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // TODO: implement RD MTL replay
  }
  return true;
}

void WrappedMTLDevice::CaptureCmdBufSubmit(MetalResourceRecord *record)
{
  RDCASSERTEQUAL(record->cmdInfo->status, MetalCmdBufferStatus::Submitted);
  RDCASSERT(IsCaptureMode(m_State));
  WrappedMTLCommandBuffer *commandBuffer = (WrappedMTLCommandBuffer *)(record->m_Resource);
  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;
    std::unordered_set<ResourceId> refIDs;
    // The record will get deleted at the end of active frame capture
    record->AddRef();
    record->AddReferencedIDs(refIDs);
    // snapshot/detect any CPU modifications to the contents
    // of referenced MTLBuffer with shared storage mode
    for(auto it = refIDs.begin(); it != refIDs.end(); ++it)
    {
      ResourceId id = *it;
      MetalResourceRecord *refRecord = GetResourceManager()->GetResourceRecord(id);
      if(refRecord->m_Type == eResBuffer)
      {
        // TODO: capture CPU modified buffers
      }
    }
    record->MarkResourceFrameReferenced(GetResID(commandBuffer->GetCommandQueue()), eFrameRef_Read);
    // pull in frame refs from this command buffer
    record->AddResourceReferences(GetResourceManager());
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_commit);
      commandBuffer->Serialise_commit(ser);
      chunk = scope.Get();
    }
    record->AddChunk(chunk);
    m_CaptureCommandBuffersSubmitted.push_back(record);
  }
  else
  {
    // Remove the reference on the real resource added during commit()
    Unwrap(commandBuffer)->release();
  }
  if(record->cmdInfo->presented)
  {
    AdvanceFrame();
    Present(record);
  }
  // In background or active capture mode the record reference is incremented in
  // CaptureCmdBufEnqueue
  record->Delete(GetResourceManager());
}

void WrappedMTLDevice::CaptureCmdBufCommit(MetalResourceRecord *cbRecord)
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  if(cbRecord->cmdInfo->status != MetalCmdBufferStatus::Enqueued)
    CaptureCmdBufEnqueue(cbRecord);

  RDCASSERTEQUAL(cbRecord->cmdInfo->status, MetalCmdBufferStatus::Enqueued);
  cbRecord->cmdInfo->status = MetalCmdBufferStatus::Committed;

  size_t countSubmitted = 0;
  for(MetalResourceRecord *record : m_CaptureCommandBuffersEnqueued)
  {
    if(record->cmdInfo->status == MetalCmdBufferStatus::Committed)
    {
      record->cmdInfo->status = MetalCmdBufferStatus::Submitted;
      ++countSubmitted;
      CaptureCmdBufSubmit(record);
      continue;
    }
    break;
  };
  m_CaptureCommandBuffersEnqueued.erase(0, countSubmitted);
}

void WrappedMTLDevice::CaptureCmdBufEnqueue(MetalResourceRecord *cbRecord)
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  RDCASSERTEQUAL(cbRecord->cmdInfo->status, MetalCmdBufferStatus::Unknown);
  cbRecord->cmdInfo->status = MetalCmdBufferStatus::Enqueued;
  cbRecord->AddRef();
  m_CaptureCommandBuffersEnqueued.push_back(cbRecord);

  RDCDEBUG("Enqueing CommandBufferRecord %s %d", ToStr(cbRecord->GetResourceID()).c_str(),
           m_CaptureCommandBuffersEnqueued.count());
}

void WrappedMTLDevice::AdvanceFrame()
{
  if(IsBackgroundCapturing(m_State))
    RenderDoc::Inst().Tick();

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame
}

void WrappedMTLDevice::FirstFrame()
{
  // if we have to capture the first frame, begin capturing immediately
  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    RenderDoc::Inst().StartFrameCapture(DeviceOwnedWindow(this, NULL));

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = 0;
  }
}

void WrappedMTLDevice::Present(MetalResourceRecord *record)
{
  WrappedMTLTexture *backBuffer = record->cmdInfo->backBuffer;
  {
    SCOPED_LOCK(m_CapturePotentialBackBuffersLock);
    if(m_CapturePotentialBackBuffers.count(backBuffer) == 0)
    {
      RDCERR("Capture ignoring Present called on unknown backbuffer");
      return;
    }
  }

  CA::MetalLayer *outputLayer = record->cmdInfo->outputLayer;
  DeviceOwnedWindow devWnd(this, outputLayer);

  bool activeWindow = RenderDoc::Inst().IsActiveWindow(devWnd);

  RenderDoc::Inst().AddActiveDriver(RDCDriver::Metal, true);

  if(!activeWindow)
    return;

  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture(devWnd);

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && IsBackgroundCapturing(m_State))
  {
    RenderDoc::Inst().StartFrameCapture(devWnd);

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = m_FrameCounter;
  }
}

void WrappedMTLDevice::CaptureClearSubmittedCmdBuffers()
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
  {
    record->Delete(GetResourceManager());
  }

  m_CaptureCommandBuffersSubmitted.clear();
}

void WrappedMTLDevice::RegisterMetalLayer(CA::MetalLayer *mtlLayer)
{
  SCOPED_LOCK(m_CaptureOutputLayersLock);
  if(m_CaptureOutputLayers.count(mtlLayer) == 0)
  {
    m_CaptureOutputLayers.insert(mtlLayer);
    TrackedCAMetalLayer::Track(mtlLayer, this);

    DeviceOwnedWindow devWnd(this, mtlLayer);
    RenderDoc::Inst().AddFrameCapturer(devWnd, &m_Capturer);
  }
}

void WrappedMTLDevice::UnregisterMetalLayer(CA::MetalLayer *mtlLayer)
{
  SCOPED_LOCK(m_CaptureOutputLayersLock);
  RDCASSERT(m_CaptureOutputLayers.count(mtlLayer));
  m_CaptureOutputLayers.erase(mtlLayer);

  DeviceOwnedWindow devWnd(this, mtlLayer);
  RenderDoc::Inst().RemoveFrameCapturer(devWnd);
}

MetalInitParams::MetalInitParams()
{
  memset(this, 0, sizeof(MetalInitParams));
}

uint64_t MetalInitParams::GetSerialiseSize()
{
  size_t ret = sizeof(*this);
  return (uint64_t)ret;
}

void MetalInitParams::Set(MTL::Device *pRealDevice, ResourceId device)
{
  DeviceID = device;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MetalInitParams &el)
{
  SERIALISE_MEMBER(DeviceID).TypedAs("MTLDevice"_lit);
}

INSTANTIATE_SERIALISE_TYPE(MetalInitParams);
