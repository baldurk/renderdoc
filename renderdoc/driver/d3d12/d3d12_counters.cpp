/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
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

#include "d3d12_command_queue.h"
#include "d3d12_common.h"
#include "d3d12_device.h"

void D3D12Replay::PreDeviceInitCounters()
{
}

void D3D12Replay::PostDeviceInitCounters()
{
}

void D3D12Replay::PreDeviceShutdownCounters()
{
}

void D3D12Replay::PostDeviceShutdownCounters()
{
}

vector<uint32_t> D3D12Replay::EnumerateCounters()
{
  vector<uint32_t> ret;

  ret.push_back(eCounter_EventGPUDuration);
  ret.push_back(eCounter_InputVerticesRead);
  ret.push_back(eCounter_IAPrimitives);
  ret.push_back(eCounter_VSInvocations);
  ret.push_back(eCounter_GSInvocations);
  ret.push_back(eCounter_GSPrimitives);
  ret.push_back(eCounter_CInvocations);
  ret.push_back(eCounter_RasterizedPrimitives);
  ret.push_back(eCounter_PSInvocations);
  ret.push_back(eCounter_HSInvocations);
  ret.push_back(eCounter_DSInvocations);
  ret.push_back(eCounter_CSInvocations);
  ret.push_back(eCounter_SamplesWritten);

  return ret;
}

void D3D12Replay::DescribeCounter(uint32_t counterID, CounterDescription &desc)
{
  desc.counterID = counterID;

  switch (counterID)
  {
    case eCounter_EventGPUDuration:
      desc.name = "GPU Duration";
      desc.description =
        "Time taken for this event on the GPU, as measured by delta between two GPU timestamps.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_Double;
      desc.units = eUnits_Seconds;
      break;
    case eCounter_InputVerticesRead:
      desc.name = "Input Vertices Read";
      desc.description = "Number of vertices read by input assembler.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_IAPrimitives:
      desc.name = "Input Primitives";
      desc.description = "Number of primitives read by the input assembler.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_VSInvocations:
      desc.name = "VS Invocations";
      desc.description = "Number of times a vertex shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_GSInvocations:
      desc.name = "GS Invocations";
      desc.description = "Number of times a geometry shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_GSPrimitives:
      desc.name = "GS Primitives";
      desc.description = "Number of primitives output by a geometry shader.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_CInvocations:
      desc.name = "CInvocations";
      desc.description = "Number of primitives that were sent to the rasterizer.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_RasterizedPrimitives:
      desc.name = "Rasterized Primitives";
      desc.description = "Number of primitives that were rendered.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_PSInvocations:
      desc.name = "PS Invocations";
      desc.description = "Number of times a pixel shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_HSInvocations:
      desc.name = "HS Invocations";
      desc.description = "Number of times a hull shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_DSInvocations:
      desc.name = "DS Invocations";
      desc.description = "Number of times a domain shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_CSInvocations:
      desc.name = "CS Invocations";
      desc.description = "Number of times a compute shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_SamplesWritten:
      desc.name = "Samples Written";
      desc.description = "Number of samples that passed depth/stencil test.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    default:
      desc.name = "Unknown";
      desc.description = "Unknown counter ID";
      desc.resultByteWidth = 0;
      desc.resultCompType = eCompType_None;
      desc.units = eUnits_Absolute;
      break;
  }
}

struct D3D12GPUTimerCallback : public D3D12DrawcallCallback
{
  D3D12GPUTimerCallback(WrappedID3D12Device *dev, D3D12Replay *rp, ID3D12QueryHeap *qh)
      : m_pDevice(dev), m_pReplay(rp), m_QueryHeap(qh)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = this;
  }
  ~D3D12GPUTimerCallback() { m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = NULL; }
  void PreDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    cmd->EndQuery(m_QueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, (uint32_t)(m_Results.size() * 2 + 0));
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    cmd->EndQuery(m_QueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, (uint32_t)(m_Results.size() * 2 + 1));
    m_Results.push_back(eid);
    return false;
  }

  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandList *cmd) {}
  // we don't need to distinguish, call the Draw functions
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) { PreDraw(eid, cmd); }
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) { return PostDraw(eid, cmd); }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) { PostRedraw(eid, cmd); }
  bool RecordAllCmds() { return true; }
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    m_AliasEvents.push_back(std::make_pair(primary, alias));
  }

  WrappedID3D12Device *m_pDevice;
  D3D12Replay *m_pReplay;
  ID3D12QueryHeap *m_QueryHeap;
  vector<uint32_t> m_Results;
  // events which are the 'same' from being the same command buffer resubmitted
  // multiple times in the frame. We will only get the full callback when we're
  // recording the command buffer, and will be given the first EID. After that
  // we'll just be told which other EIDs alias this event.
  vector<pair<uint32_t, uint32_t> > m_AliasEvents;
};

vector<CounterResult> D3D12Replay::FetchCounters(const vector<uint32_t> &counters)
{
  uint32_t maxEID = m_pDevice->GetQueue()->GetMaxEID();

  vector<CounterResult> ret;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_READBACK;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC bufDesc;
  bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufDesc.Alignment = 0;
  bufDesc.Width = sizeof(uint64_t) * maxEID * 2;
  bufDesc.Height = 1;
  bufDesc.DepthOrArraySize = 1;
  bufDesc.MipLevels = 1;
  bufDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufDesc.SampleDesc.Count = 1;
  bufDesc.SampleDesc.Quality = 0;
  bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  ID3D12Resource *readbackBuf;
  HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                  __uuidof(ID3D12Resource), (void **)&readbackBuf);
  if(FAILED(hr))
  {
    RDCERR("Failed to create query readback buffer %08x", hr);
    return ret;
  }

  D3D12_QUERY_HEAP_DESC queryDesc;
  queryDesc.Count = maxEID * 2;
  queryDesc.NodeMask = 1;
  queryDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
  ID3D12QueryHeap *queryHeap = NULL;
  hr = m_pDevice->CreateQueryHeap(&queryDesc, __uuidof(queryHeap), (void **)&queryHeap);
  if(FAILED(hr))
  {
    RDCERR("Failed to create query heap %08x", hr);
    return ret;
  }

  m_pDevice->SetStablePowerState(TRUE);

  D3D12GPUTimerCallback cb(m_pDevice, this, queryHeap);

  // replay the events to perform all the queries
  m_pDevice->ReplayLog(0, maxEID, eReplay_Full);

  m_pDevice->SetStablePowerState(FALSE);

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

  list->ResolveQueryData(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, maxEID * 2, readbackBuf, 0);

  list->Close();

  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists();

  D3D12_RANGE range = {0, (SIZE_T)bufDesc.Width};
  void *data;
  hr = readbackBuf->Map(0, &range, &data);
  if(FAILED(hr))
  {
    RDCERR("Failed to create query heap %08x", hr);
    SAFE_RELEASE(queryHeap);
    SAFE_RELEASE(readbackBuf);
    return ret;
  }

  uint64_t *timestamps = (uint64_t *)data;

  uint64_t freq;
  m_pDevice->GetQueue()->GetTimestampFrequency(&freq);

  for(size_t i = 0; i < cb.m_Results.size(); i++)
  {
    CounterResult result;

    uint64_t delta = timestamps[i * 2 + 1] - timestamps[i * 2 + 0];

    result.eventID = cb.m_Results[i];
    result.counterID = eCounter_EventGPUDuration;
    result.value.d = double(delta) / double(freq);

    ret.push_back(result);
  }

  for(size_t i = 0; i < cb.m_AliasEvents.size(); i++)
  {
    CounterResult search;
    search.counterID = eCounter_EventGPUDuration;
    search.eventID = cb.m_AliasEvents[i].first;

    // find the result we're aliasing
    auto it = std::find(ret.begin(), ret.end(), search);
    RDCASSERT(it != ret.end());

    // duplicate the result and append
    CounterResult aliased = *it;
    aliased.eventID = cb.m_AliasEvents[i].second;
    ret.push_back(aliased);
  }

  // sort so that the alias results appear in the right places
  std::sort(ret.begin(), ret.end());

  return ret;
}
