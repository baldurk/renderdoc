/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

vector<GPUCounter> D3D12Replay::EnumerateCounters()
{
  vector<GPUCounter> ret;

  ret.push_back(GPUCounter::EventGPUDuration);
  ret.push_back(GPUCounter::InputVerticesRead);
  ret.push_back(GPUCounter::IAPrimitives);
  ret.push_back(GPUCounter::GSPrimitives);
  ret.push_back(GPUCounter::RasterizerInvocations);
  ret.push_back(GPUCounter::RasterizedPrimitives);
  ret.push_back(GPUCounter::SamplesWritten);
  ret.push_back(GPUCounter::VSInvocations);
  ret.push_back(GPUCounter::HSInvocations);
  ret.push_back(GPUCounter::DSInvocations);
  ret.push_back(GPUCounter::GSInvocations);
  ret.push_back(GPUCounter::PSInvocations);
  ret.push_back(GPUCounter::CSInvocations);

  return ret;
}

void D3D12Replay::DescribeCounter(GPUCounter counterID, CounterDescription &desc)
{
  desc.counterID = counterID;

  switch(counterID)
  {
    case GPUCounter::EventGPUDuration:
      desc.name = "GPU Duration";
      desc.description =
          "Time taken for this event on the GPU, as measured by delta between two GPU timestamps.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::Double;
      desc.unit = CounterUnit::Seconds;
      break;
    case GPUCounter::InputVerticesRead:
      desc.name = "Input Vertices Read";
      desc.description = "Number of vertices read by input assembler.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::IAPrimitives:
      desc.name = "Input Primitives";
      desc.description = "Number of primitives read by the input assembler.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::GSPrimitives:
      desc.name = "GS Primitives";
      desc.description = "Number of primitives output by a geometry shader.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::RasterizerInvocations:
      desc.name = "Rasterizer Invocations";
      desc.description = "Number of primitives that were sent to the rasterizer.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::RasterizedPrimitives:
      desc.name = "Rasterized Primitives";
      desc.description = "Number of primitives that were rendered.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::SamplesWritten:
      desc.name = "Samples Written";
      desc.description = "Number of samples that passed depth/stencil test.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::VSInvocations:
      desc.name = "VS Invocations";
      desc.description = "Number of times a vertex shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::GSInvocations:
      desc.name = "GS Invocations";
      desc.description = "Number of times a geometry shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::HSInvocations:
      desc.name = "HS Invocations";
      desc.description = "Number of times a hull shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::DSInvocations:
      desc.name = "DS Invocations";
      desc.description =
          "Number of times a domain shader (or tesselation evaluation shader in OpenGL) was "
          "invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::PSInvocations:
      desc.name = "PS Invocations";
      desc.description = "Number of times a pixel shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::CSInvocations:
      desc.name = "CS Invocations";
      desc.description = "Number of times a compute shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    default:
      desc.name = "Unknown";
      desc.description = "Unknown counter ID";
      desc.resultByteWidth = 0;
      desc.resultType = CompType::Typeless;
      desc.unit = CounterUnit::Absolute;
      break;
  }
}

struct D3D12GPUTimerCallback : public D3D12DrawcallCallback
{
  D3D12GPUTimerCallback(WrappedID3D12Device *dev, D3D12Replay *rp, ID3D12QueryHeap *tqh,
                        ID3D12QueryHeap *psqh, ID3D12QueryHeap *oqh)
      : m_pDevice(dev),
        m_pReplay(rp),
        m_TimerQueryHeap(tqh),
        m_PipeStatsQueryHeap(psqh),
        m_OcclusionQueryHeap(oqh),
        m_NumStatsQueries(0),
        m_NumTimestampQueries(0)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = this;
  }
  ~D3D12GPUTimerCallback() { m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = NULL; }
  void PreDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    if(cmd->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
      cmd->BeginQuery(m_OcclusionQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, m_NumStatsQueries);
      cmd->BeginQuery(m_PipeStatsQueryHeap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, m_NumStatsQueries);
    }
    cmd->EndQuery(m_TimerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, m_NumTimestampQueries * 2 + 0);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    cmd->EndQuery(m_TimerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, m_NumTimestampQueries * 2 + 1);
    m_NumTimestampQueries++;

    bool direct = (cmd->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT);
    if(direct)
    {
      cmd->EndQuery(m_PipeStatsQueryHeap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, m_NumStatsQueries);
      cmd->EndQuery(m_OcclusionQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, m_NumStatsQueries);

      m_NumStatsQueries++;
    }
    m_Results.push_back(std::make_pair(eid, direct));
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
  ID3D12QueryHeap *m_TimerQueryHeap;
  ID3D12QueryHeap *m_PipeStatsQueryHeap;
  ID3D12QueryHeap *m_OcclusionQueryHeap;
  vector<pair<uint32_t, bool> > m_Results;

  uint32_t m_NumStatsQueries;
  uint32_t m_NumTimestampQueries;

  // events which are the 'same' from being the same command buffer resubmitted
  // multiple times in the frame. We will only get the full callback when we're
  // recording the command buffer, and will be given the first EID. After that
  // we'll just be told which other EIDs alias this event.
  vector<pair<uint32_t, uint32_t> > m_AliasEvents;
};

vector<CounterResult> D3D12Replay::FetchCounters(const vector<GPUCounter> &counters)
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
  bufDesc.Width = (sizeof(uint64_t) * 3 + sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS)) * maxEID;
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

  D3D12_QUERY_HEAP_DESC timerQueryDesc;
  timerQueryDesc.Count = maxEID * 2;
  timerQueryDesc.NodeMask = 1;
  timerQueryDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
  ID3D12QueryHeap *timerQueryHeap = NULL;
  hr = m_pDevice->CreateQueryHeap(&timerQueryDesc, __uuidof(timerQueryHeap),
                                  (void **)&timerQueryHeap);
  if(FAILED(hr))
  {
    RDCERR("Failed to create timer query heap %08x", hr);
    return ret;
  }

  D3D12_QUERY_HEAP_DESC pipestatsQueryDesc;
  pipestatsQueryDesc.Count = maxEID;
  pipestatsQueryDesc.NodeMask = 1;
  pipestatsQueryDesc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
  ID3D12QueryHeap *pipestatsQueryHeap = NULL;
  hr = m_pDevice->CreateQueryHeap(&pipestatsQueryDesc, __uuidof(pipestatsQueryHeap),
                                  (void **)&pipestatsQueryHeap);
  if(FAILED(hr))
  {
    RDCERR("Failed to create pipeline statistics query heap %08x", hr);
    return ret;
  }

  D3D12_QUERY_HEAP_DESC occlusionQueryDesc;
  occlusionQueryDesc.Count = maxEID;
  occlusionQueryDesc.NodeMask = 1;
  occlusionQueryDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
  ID3D12QueryHeap *occlusionQueryHeap = NULL;
  hr = m_pDevice->CreateQueryHeap(&occlusionQueryDesc, __uuidof(occlusionQueryHeap),
                                  (void **)&occlusionQueryHeap);
  if(FAILED(hr))
  {
    RDCERR("Failed to create occlusion query heap %08x", hr);
    return ret;
  }

  // Only supported with developer mode drivers!!!
  hr = m_pDevice->SetStablePowerState(TRUE);
  if(FAILED(hr))
    MessageBoxA(NULL,
                "D3D12 counters require Win10 developer mode enabled: Settings > Update & Security "
                "> For Developers > Developer Mode",
                "D3D12 Counters Error", MB_ICONWARNING | MB_OK);

  D3D12GPUTimerCallback cb(m_pDevice, this, timerQueryHeap, pipestatsQueryHeap, occlusionQueryHeap);

  // replay the events to perform all the queries
  m_pDevice->ReplayLog(0, maxEID, eReplay_Full);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists(true);
#endif

  // Only supported with developer mode drivers!!!
  m_pDevice->SetStablePowerState(FALSE);

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

  UINT64 bufferOffset = 0;

  list->ResolveQueryData(timerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0,
                         cb.m_NumTimestampQueries * 2, readbackBuf, bufferOffset);

  bufferOffset += sizeof(uint64_t) * 2 * cb.m_NumTimestampQueries;

  list->ResolveQueryData(pipestatsQueryHeap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0,
                         cb.m_NumStatsQueries, readbackBuf, bufferOffset);

  bufferOffset += sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS) * cb.m_NumStatsQueries;

  list->ResolveQueryData(occlusionQueryHeap, D3D12_QUERY_TYPE_OCCLUSION, 0, cb.m_NumStatsQueries,
                         readbackBuf, bufferOffset);

  list->Close();

  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists();

  D3D12_RANGE range;
  range.Begin = 0;
  range.End = (SIZE_T)bufDesc.Width;

  uint8_t *data;
  hr = readbackBuf->Map(0, &range, (void **)&data);
  if(FAILED(hr))
  {
    RDCERR("Failed to read timer query heap data %08x", hr);
    SAFE_RELEASE(readbackBuf);
    SAFE_RELEASE(timerQueryHeap);
    SAFE_RELEASE(pipestatsQueryHeap);
    SAFE_RELEASE(occlusionQueryHeap);
    return ret;
  }

  uint64_t *timestamps = (uint64_t *)data;
  data += cb.m_NumTimestampQueries * 2 * sizeof(uint64_t);
  D3D12_QUERY_DATA_PIPELINE_STATISTICS *pipelinestats = (D3D12_QUERY_DATA_PIPELINE_STATISTICS *)data;
  data += cb.m_NumStatsQueries * sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);
  uint64_t *occlusion = (uint64_t *)data;

  uint64_t freq;
  m_pDevice->GetQueue()->GetTimestampFrequency(&freq);

  for(size_t i = 0; i < cb.m_Results.size(); i++)
  {
    bool direct = cb.m_Results[i].second;

    D3D12_QUERY_DATA_PIPELINE_STATISTICS pipeStats = {};
    uint64_t occl = 0;

    // only events on direct lists recorded pipeline stats or occlusion queries
    if(direct)
    {
      pipeStats = *pipelinestats;
      occl = *occlusion;

      pipelinestats++;
      occlusion++;
    }

    for(size_t c = 0; c < counters.size(); c++)
    {
      CounterResult result;

      result.eventID = cb.m_Results[i].first;
      result.counterID = counters[c];

      switch(counters[c])
      {
        case GPUCounter::EventGPUDuration:
        {
          uint64_t delta = timestamps[i * 2 + 1] - timestamps[i * 2 + 0];
          result.value.d = double(delta) / double(freq);
        }
        break;
        case GPUCounter::InputVerticesRead: result.value.u64 = pipeStats.IAVertices; break;
        case GPUCounter::IAPrimitives: result.value.u64 = pipeStats.IAPrimitives; break;
        case GPUCounter::GSPrimitives: result.value.u64 = pipeStats.GSPrimitives; break;
        case GPUCounter::RasterizerInvocations: result.value.u64 = pipeStats.CInvocations; break;
        case GPUCounter::RasterizedPrimitives: result.value.u64 = pipeStats.CPrimitives; break;
        case GPUCounter::SamplesWritten: result.value.u64 = occl; break;
        case GPUCounter::VSInvocations: result.value.u64 = pipeStats.VSInvocations; break;
        case GPUCounter::HSInvocations: result.value.u64 = pipeStats.HSInvocations; break;
        case GPUCounter::DSInvocations: result.value.u64 = pipeStats.DSInvocations; break;
        case GPUCounter::GSInvocations: result.value.u64 = pipeStats.GSInvocations; break;
        case GPUCounter::PSInvocations: result.value.u64 = pipeStats.PSInvocations; break;
        case GPUCounter::CSInvocations: result.value.u64 = pipeStats.CSInvocations; break;
      }
      ret.push_back(result);
    }
  }

  for(size_t i = 0; i < cb.m_AliasEvents.size(); i++)
  {
    for(size_t c = 0; c < counters.size(); c++)
    {
      CounterResult search;
      search.counterID = counters[c];
      search.eventID = cb.m_AliasEvents[i].first;

      // find the result we're aliasing
      auto it = std::find(ret.begin(), ret.end(), search);
      RDCASSERT(it != ret.end());

      // duplicate the result and append
      CounterResult aliased = *it;
      aliased.eventID = cb.m_AliasEvents[i].second;
      ret.push_back(aliased);
    }
  }

  // sort so that the alias results appear in the right places
  std::sort(ret.begin(), ret.end());

  SAFE_RELEASE(readbackBuf);
  SAFE_RELEASE(timerQueryHeap);
  SAFE_RELEASE(pipestatsQueryHeap);
  SAFE_RELEASE(occlusionQueryHeap);

  return ret;
}
