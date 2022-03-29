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
#include "metal_device.h"
#include "metal_resources.h"

#include "core/settings.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_library.h"
#include "metal_render_command_encoder.h"

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

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_BeginCaptureFrame(SerialiserType &ser)
{
  // TODO: serialise image references and states

  SERIALISE_CHECK_READ_ERRORS();

  return true;
}

void WrappedMTLDevice::StartFrameCapture(void *dev, void *wnd)
{
  if(!IsBackgroundCapturing(m_State))
    return;

  RDCLOG("Starting capture");

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
    // TODO: sync all active command buffers
    GetResourceManager()->PrepareInitialContents();

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();
    // TODO: handle image states
    m_State = CaptureState::ActiveCapturing;
  }

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(this), eFrameRef_Read);
  // TODO: do resources referenced by the command queues & buffers need to be marked

  // TODO: is there any other type of resource that needs to be marked as frame referenced
}

void WrappedMTLDevice::EndCaptureFrame()
{
  CACHE_THREAD_SERIALISER();
  ser.SetActionChunk();
  SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureEnd);

  // TODO: serialise the presented image

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

bool WrappedMTLDevice::EndFrameCapture(void *dev, void *wnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  // TODO: find the window and drawable being captured

  RDCLOG("Finished capture, Frame %u", m_CapturedFrames.back().frameNumber);

  // TODO: mark the drawable and its images as frame referenced

  // transition back to IDLE atomically
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);
    EndCaptureFrame();

    m_State = CaptureState::BackgroundCapturing;

    // TODO: wait for the GPU to be idle
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
      m_InitParams.Set(Unwrap(this), id);
      SCOPED_SERIALISE_CHUNK(SystemChunk::DriverInit, m_InitParams.GetSerialiseSize());

      SERIALISE_ELEMENT(m_InitParams);
    }

    RDCDEBUG("Inserting Resource Serialisers");

    GetResourceManager()->InsertReferencedChunks(ser);

    GetResourceManager()->InsertInitialContentsChunks(ser);

    RDCDEBUG("Creating Capture Scope");

    GetResourceManager()->Serialise_InitialContentsNeeded(ser);
    // GetResourceManager()->InsertDeviceMemoryRefs(ser);

    {
      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureScope, 16);
      Serialise_CaptureScope(ser);
    }

    {
      WriteSerialiser &captureBeginSer = GetThreadSerialiser();
      ScopedChunk scope(captureBeginSer, SystemChunk::CaptureBegin);

      Serialise_BeginCaptureFrame(captureBeginSer);
      m_HeaderChunk = scope.Get();
    }

    m_HeaderChunk->Write(ser);

    // don't need to lock access to m_CmdBufferRecords as we are no longer
    // in capframe (the transition is thread-protected) so nothing will be
    // pushed to the vector

    {
      std::map<int64_t, Chunk *> recordlist;
      RDCDEBUG("Flushing %zu command buffer records to file serialiser", m_CmdBufferRecords.size());
      // ensure all command buffer records within the frame even if recorded before, but
      // otherwise order must be preserved (vs. queue submits and desc set updates)
      for(size_t i = 0; i < m_CmdBufferRecords.size(); i++)
      {
        RDCDEBUG("Adding chunks from command buffer %s",
                 ToStr(m_CmdBufferRecords[i]->GetResourceID()).c_str());

        size_t prevSize = recordlist.size();
        (void)prevSize;

        m_CmdBufferRecords[i]->Insert(recordlist);

        RDCDEBUG("Added %zu chunks to file serialiser", recordlist.size() - prevSize);
      }

      size_t prevSize = recordlist.size();
      (void)prevSize;
      m_FrameCaptureRecord->Insert(recordlist);
      RDCDEBUG("Adding %zu frame capture chunks to file serialiser", recordlist.size() - prevSize);
      RDCDEBUG("Flushing %zu chunks to file serialiser from context record", recordlist.size());

      float num = float(recordlist.size());
      float idx = 0.0f;

      for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
      {
        RenderDoc::Inst().SetProgress(CaptureProgress::SerialiseFrameContents, idx / num);
        idx += 1.0f;
        it->second->Write(ser);
        RDCLOG("Writing Chunk %d", (int)it->second->GetChunkType<MetalChunk>());
      }

      RDCDEBUG("Done");
    }

    captureSectionSize = captureWriter->GetOffset();
  }

  RDCLOG("Captured Metal frame with %f MB capture section in %f seconds",
         double(captureSectionSize) / (1024.0 * 1024.0), m_CaptureTimer.GetMilliseconds() / 1000.0);

  RenderDoc::Inst().FinishCaptureWriting(rdc, m_CapturedFrames.back().frameNumber);

  m_HeaderChunk->Delete();
  m_HeaderChunk = NULL;

  m_State = CaptureState::BackgroundCapturing;

  // delete cmd buffers now - had to keep them alive until after serialiser flush.
  for(size_t i = 0; i < m_CmdBufferRecords.size(); i++)
    m_CmdBufferRecords[i]->Delete(GetResourceManager());

  m_CmdBufferRecords.clear();

  GetResourceManager()->ResetLastWriteTimes();

  GetResourceManager()->MarkUnwrittenResources();

  // TODO: handle memory resources in the resource manager

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  // TODO: handle memory resources in the initial contents
  return true;
}

bool WrappedMTLDevice::DiscardFrameCapture(void *dev, void *wnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  RDCLOG("Discarding frame capture.");

  RenderDoc::Inst().FinishCaptureWriting(NULL, m_CapturedFrames.back().frameNumber);

  m_CapturedFrames.pop_back();

  // transition back to IDLE atomically
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);

    m_State = CaptureState::BackgroundCapturing;

    /*
    ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

    {
      SCOPED_LOCK(m_CoherentMapsLock);
      for(auto it = m_CoherentMaps.begin(); it != m_CoherentMaps.end(); ++it)
      {
        FreeAlignedBuffer((*it)->memMapState->refData);
        (*it)->memMapState->refData = NULL;
        (*it)->memMapState->needRefData = false;
      }
    }
     */
  }

  m_HeaderChunk->Delete();
  m_HeaderChunk = NULL;

  // delete cmd buffers now - had to keep them alive until after serialiser flush.
  for(size_t i = 0; i < m_CmdBufferRecords.size(); i++)
    m_CmdBufferRecords[i]->Delete(GetResourceManager());

  m_CmdBufferRecords.clear();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  //  FreeAllMemory(MemoryScope::InitialContents);

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

void WrappedMTLDevice::AddCommandBufferRecord(MetalResourceRecord *record)
{
  SCOPED_LOCK(m_CmdBufferRecordsLock);
  m_CmdBufferRecords.push_back(record);
  RDCDEBUG("Adding CommandBufferRecord Count %zu", m_CmdBufferRecords.size());
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
    RenderDoc::Inst().StartFrameCapture(this, NULL);

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = 0;
  }
}

void WrappedMTLDevice::Present(void *wnd)
{
  bool activeWindow = wnd == NULL || RenderDoc::Inst().IsActiveWindow(this, wnd);

  RenderDoc::Inst().AddActiveDriver(RDCDriver::Metal, true);

  if(!activeWindow)
    return;

  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture(this, wnd);

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && IsBackgroundCapturing(m_State))
  {
    RenderDoc::Inst().StartFrameCapture(this, wnd);

    m_AppControlledCapture = false;

    FrameDescription frame;
    frame.frameNumber = m_AppControlledCapture ? ~0U : m_FrameCounter;
    frame.captureTime = Timing::GetUnixTimestamp();
    m_CapturedFrames.push_back(frame);

    m_CapturedFrames.back().frameNumber = m_FrameCounter;
  }
}

MetalInitParams::MetalInitParams()
{
  memset(this, 0, sizeof(MetalInitParams));
}

uint64_t MetalInitParams::GetSerialiseSize()
{
  size_t ret = 0;

  // device information
  ret += sizeof(uint32_t);
  ret += sizeof(char) * name->length();

  ret += sizeof(uint64_t) * 4 + sizeof(uint32_t) * 2;
  ret += sizeof(MTL::DeviceLocation);
  ret += sizeof(NS::UInteger);
  ret += sizeof(bool) * 4;

  // device capabilities
  ret += sizeof(bool) * 15;
  ret += sizeof(MTL::ArgumentBuffersTier);

  ret += sizeof(ResourceId);

  return (uint64_t)ret;
}

void MetalInitParams::Set(MTL::Device *pRealDevice, ResourceId inst)
{
  // device information
  name = pRealDevice->name();
  recommendedMaxWorkingSetSize = pRealDevice->recommendedMaxWorkingSetSize();
  maxTransferRate = pRealDevice->maxTransferRate();
  registryID = pRealDevice->registryID();
  peerGroupID = pRealDevice->peerGroupID();
  peerCount = pRealDevice->peerCount();
  peerIndex = pRealDevice->peerIndex();
  location = pRealDevice->location();
  locationNumber = pRealDevice->locationNumber();
  hasUnifiedMemory = pRealDevice->hasUnifiedMemory();
  headless = pRealDevice->headless();
  lowPower = pRealDevice->lowPower();
  removable = pRealDevice->removable();

  // device capabilities
  supportsMTLGPUFamilyCommon1 = pRealDevice->supportsFamily(MTL::GPUFamilyCommon1);
  supportsMTLGPUFamilyCommon2 = pRealDevice->supportsFamily(MTL::GPUFamilyCommon2);
  supportsMTLGPUFamilyCommon3 = pRealDevice->supportsFamily(MTL::GPUFamilyCommon3);

  supportsMTLGPUFamilyApple1 = pRealDevice->supportsFamily(MTL::GPUFamilyApple1);
  supportsMTLGPUFamilyApple2 = pRealDevice->supportsFamily(MTL::GPUFamilyApple2);
  supportsMTLGPUFamilyApple3 = pRealDevice->supportsFamily(MTL::GPUFamilyApple3);
  supportsMTLGPUFamilyApple4 = pRealDevice->supportsFamily(MTL::GPUFamilyApple4);
  supportsMTLGPUFamilyApple5 = pRealDevice->supportsFamily(MTL::GPUFamilyApple5);
  supportsMTLGPUFamilyApple6 = pRealDevice->supportsFamily(MTL::GPUFamilyApple6);
  supportsMTLGPUFamilyApple7 = pRealDevice->supportsFamily(MTL::GPUFamilyApple7);
  supportsMTLGPUFamilyApple8 = pRealDevice->supportsFamily(MTL::GPUFamilyApple8);

  supportsMTLGPUFamilyMac1 = pRealDevice->supportsFamily(MTL::GPUFamilyMac1);
  supportsMTLGPUFamilyMac2 = pRealDevice->supportsFamily(MTL::GPUFamilyMac2);

  supportsMTLGPUFamilyMacCatalyst1 = pRealDevice->supportsFamily(MTL::GPUFamilyMacCatalyst1);
  supportsMTLGPUFamilyMacCatalyst2 = pRealDevice->supportsFamily(MTL::GPUFamilyMacCatalyst2);

  argumentBuffersSupport = pRealDevice->argumentBuffersSupport();

  InstanceID = inst;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MetalInitParams &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(recommendedMaxWorkingSetSize);
  SERIALISE_MEMBER(maxTransferRate);
  SERIALISE_MEMBER(registryID);
  SERIALISE_MEMBER(peerGroupID);
  SERIALISE_MEMBER(peerCount);
  SERIALISE_MEMBER(peerIndex);
  SERIALISE_MEMBER(location);
  SERIALISE_MEMBER(locationNumber);
  SERIALISE_MEMBER(hasUnifiedMemory);
  SERIALISE_MEMBER(headless);
  SERIALISE_MEMBER(lowPower);
  SERIALISE_MEMBER(removable);

  // device capabilities
  SERIALISE_MEMBER(supportsMTLGPUFamilyCommon1);
  SERIALISE_MEMBER(supportsMTLGPUFamilyCommon2);
  SERIALISE_MEMBER(supportsMTLGPUFamilyCommon3);

  SERIALISE_MEMBER(supportsMTLGPUFamilyApple1);
  SERIALISE_MEMBER(supportsMTLGPUFamilyApple2);
  SERIALISE_MEMBER(supportsMTLGPUFamilyApple3);
  SERIALISE_MEMBER(supportsMTLGPUFamilyApple4);
  SERIALISE_MEMBER(supportsMTLGPUFamilyApple5);
  SERIALISE_MEMBER(supportsMTLGPUFamilyApple6);
  SERIALISE_MEMBER(supportsMTLGPUFamilyApple7);
  SERIALISE_MEMBER(supportsMTLGPUFamilyApple8);

  SERIALISE_MEMBER(supportsMTLGPUFamilyMac1);
  SERIALISE_MEMBER(supportsMTLGPUFamilyMac2);

  SERIALISE_MEMBER(supportsMTLGPUFamilyMacCatalyst1);
  SERIALISE_MEMBER(supportsMTLGPUFamilyMacCatalyst2);

  SERIALISE_MEMBER(argumentBuffersSupport);
}

INSTANTIATE_SERIALISE_TYPE(MetalInitParams);
