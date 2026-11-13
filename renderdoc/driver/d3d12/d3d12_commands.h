/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2026 Baldur Karlsson
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

#include "common/common.h"
#include "d3d12_common.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"
#include "d3d12_state.h"

struct D3D12ExecuteData
{
  ID3D12Resource *argBuf = NULL;
  ID3D12Resource *countBuf = NULL;
  uint64_t argOffs = 0;
  uint64_t countOffs = 0;
  WrappedID3D12CommandSignature *sig = NULL;
  UINT maxCount = 0;
  UINT reservedCount = 0;
};

struct D3D12EventNode
{
  D3D12EventNode() = default;
  ~D3D12EventNode() { SAFE_DELETE(state); }
  D3D12EventNode(const D3D12EventNode &other) { *this = other; }
  D3D12EventNode &operator=(const D3D12EventNode &a)
  {
    event = a.event;
    action = a.action;
    SAFE_DELETE(state);
    if(a.state)
      state = new D3D12RenderState(*a.state);
    executeData = a.executeData;
    resourceUsage = a.resourceUsage;
    debugMessages = a.debugMessages;
    annotations = a.annotations;
    cmdListID = a.cmdListID;
    primaryCmdId = a.primaryCmdId;
    childCmdBufId = a.childCmdBufId;
    addActionUse = a.addActionUse;
    addPrimaryExecute = a.addPrimaryExecute;
    hasExecuteData = a.hasExecuteData;
    return *this;
  }
  D3D12EventNode &operator=(D3D12EventNode &&a)
  {
    event = a.event;
    action = a.action;
    SAFE_DELETE(state);
    state = a.state;
    a.state = NULL;
    executeData = a.executeData;
    resourceUsage = a.resourceUsage;
    debugMessages = a.debugMessages;
    annotations = a.annotations;
    cmdListID = a.cmdListID;
    primaryCmdId = a.primaryCmdId;
    childCmdBufId = a.childCmdBufId;
    addActionUse = a.addActionUse;
    addPrimaryExecute = a.addPrimaryExecute;
    hasExecuteData = a.hasExecuteData;
    return *this;
  }
  void AddResourceUsage(ResourceId id, ResourceUsage usage)
  {
    resourceUsage.push_back(make_rdcpair(id, usage));
  }

  // eventId is not used
  APIEvent event;

  ActionDescription action;

  D3D12RenderState *state = NULL;

  D3D12ExecuteData executeData;

  rdcarray<rdcpair<ResourceId, ResourceUsage>> resourceUsage;

  // eventId is not used
  rdcarray<DebugMessage> debugMessages;
  rdcarray<PendingAnnotation> annotations;

  ResourceId cmdListID;
  ResourceId primaryCmdId;
  ResourceId childCmdBufId;
  bool addActionUse = false;
  bool addPrimaryExecute = false;
  bool hasExecuteData = false;
};

struct D3D12ActionCallback
{
  // the three callbacks are used to allow the callback implementor to either
  // do a modified draw before or after the real thing.
  //
  // PreDraw()
  // do draw call as specified by the log
  // PostDraw()
  // if PostDraw() returns true:
  //   do draw call again
  //   PostRedraw()
  //
  // So either the modification happens in PreDraw, the modified draw happens,
  // then in PostDraw() the implementation can elect to undo the modifications
  // and do the real draw by returning true. OR they can do nothing in PreDraw,
  // do the real draw, then in PostDraw return true to apply the modifications
  // which are then undone in PostRedraw.
  virtual void PreDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) = 0;
  virtual bool PostDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) = 0;
  virtual void PostRedraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) = 0;

  // same principle as above, but for dispatch calls
  virtual void PreDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) = 0;
  virtual bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) = 0;
  virtual void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) = 0;

  // finally, these are for copy/blit/resolve/clear/etc
  virtual void PreMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) = 0;
  virtual bool PostMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) = 0;
  virtual void PostRemisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) = 0;

  // called immediately before a command list is closed
  virtual void PreCloseCommandList(ID3D12GraphicsCommandListX *cmd) = 0;
  // if a command list is recorded once and submitted N > 1 times, then the same
  // action will have several EIDs that refer to it. We'll only do the full
  // callbacks above for the first EID, then call this function for the others
  // to indicate that they are the same.
  virtual void AliasEvent(uint32_t primary, uint32_t alias) = 0;

  // helper functions to downcast command list because we know it's wrapped
  void PreDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    return PreDraw(eid, (ID3D12GraphicsCommandListX *)cmd);
  }
  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    return PostDraw(eid, (ID3D12GraphicsCommandListX *)cmd);
  }
  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    return PostRedraw(eid, (ID3D12GraphicsCommandListX *)cmd);
  }
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    return PreDispatch(eid, (ID3D12GraphicsCommandListX *)cmd);
  }
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    return PostDispatch(eid, (ID3D12GraphicsCommandListX *)cmd);
  }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    return PostRedispatch(eid, (ID3D12GraphicsCommandListX *)cmd);
  }
  void PreCloseCommandList(ID3D12GraphicsCommandList *cmd)
  {
    return PreCloseCommandList((ID3D12GraphicsCommandListX *)cmd);
  }
};

class WrappedID3D12CommandSignature;
struct AccStructPatchInfo;

struct BakedCmdListInfo
{
  ~BakedCmdListInfo() {}

  SubresourceStateVector GetState(WrappedID3D12Device *device, ResourceId id);

  rdcarray<D3D12EventNode> eventNodes;

  rdcarray<PendingAnnotation> pendingAnnotations;

  struct PatchRaytracing
  {
    bool m_patched = false;
    uint32_t numDescs = 0;
    D3D12GpuBuffer *patchedInstanceBuffer = NULL;
    D3D12GpuBuffer *unpatchedInstanceBufferReadback = NULL;
    ResourceId destinationAS;
  };

  rdcflatmap<uint32_t, PatchRaytracing> m_patchRaytracingInfo;

  struct OutstandingQuery
  {
    ID3D12QueryHeap *heap;
    D3D12_QUERY_TYPE Type;
    UINT Index;

    bool operator==(const OutstandingQuery &q) const
    {
      return heap == q.heap && Type == q.Type && Index == q.Index;
    }
  };

  rdcarray<OutstandingQuery> m_OutstandingQueries;

  ResourceId allocator;
  D3D12_COMMAND_LIST_TYPE type;
  UINT nodeMask;
  D3D12RenderState state;

  BarrierSet barriers;

  ResourceId parentList;

  // modified during recording to ensure we end any markers that should be ended but weren't due to
  // a partial replay
  int markerCount;

  uint32_t beginChunk = 0;
  uint32_t endChunk = 0;

  uint32_t eventCount;    // how many events are in this cmd list, for quick skipping
  uint32_t curEventID;    // current event ID while replaying, not used during loading

  bool hasExecuteDatas = false;
};

class WrappedID3D12Device;

struct D3D12CommandData
{
  D3D12CommandData();

  WrappedID3D12Device *m_pDevice;

  D3D12ActionCallback *m_ActionCallback;

  ResourceId m_LastCmdListID;

  RDResult m_FailedReplayResult = ResultCode::APIReplayFailed;

  rdcarray<ID3D12Resource *> m_IndirectBuffers;
  static const uint64_t m_IndirectSize = 4 * 1024 * 1024;
  uint64_t m_IndirectOffset;

  std::map<ResourceId, BakedCmdListInfo> m_BakedCmdListInfo;

  D3D12RenderState m_RenderState;

  rdcarray<SDObject *> m_EventAnnotations;
  SDObject *m_RootAnnotation = NULL;

  D3D12RenderState &GetCurRenderState()
  {
    return m_LastCmdListID == ResourceId() ? m_RenderState
                                           : m_BakedCmdListInfo[m_LastCmdListID].state;
  }
  enum PartialReplayIndex
  {
    Primary,
    Secondary,
    ePartialNum
  };

  // by definition, when replaying we must have N completely submitted command lists, and at most
  // two partially-submitted command lists. One primary, that we're part-way through, and then
  // if we're part-way through a ExecuteBundle inside that primary then there's one
  // secondary.
  struct PartialReplayData
  {
    PartialReplayData() { Reset(); }
    void Reset()
    {
      partialParent = ResourceId();
      baseEvent = 0;
      renderPassActive = false;
    }

    // this records where in the frame a command list was executed, so that we know if our replay
    // range ends in one of these ranges we need to construct a partial command list for future
    // replaying. Note that we always have the complete command list around - it's the bakeID
    // itself.
    // Since we only ever record a bakeID once the key is unique - note that the same command list
    // could be reset multiple times a frame, so the parent command list ID (the one recorded in
    // CmdList chunks) is NOT unique.
    // However, a single baked command list can be executed multiple times - so we have to have a
    // list of base events
    // Map from bakeID -> vector<baseEventID>
    std::map<ResourceId, rdcarray<uint32_t>> cmdListExecs;

    // This is just the baked ID of the parent command list that's partially replayed
    // If we are in the middle of a partial replay - allows fast checking in all CmdList chunks,
    // with the iteration through the above list only in Reset.
    // partialParent gets reset to ResourceId() in the Close so that other baked command lists from
    // the same parent don't pick it up
    // Also reset each overall replay
    ResourceId partialParent;

    // If a partial replay is detected, this records the base of the
    // range. This both allows easily and uniquely identifying it in the
    // executecmdlists, but also allows the recording to 'rebase' the
    // last event ID by subtracting this, to know how far to record
    uint32_t baseEvent;

    // if a render pass is active when we early-close a list, we need to end it
    bool renderPassActive;
  } m_Partial[ePartialNum];

  // if we're replaying just a single action or a particular command
  // list subsection of command events, we don't go through the
  // whole original command lists to set up the partial replay,
  // so we just set this command list
  ID3D12GraphicsCommandListX *m_OutsideCmdList = NULL;

  SDObject *InsertEventNodes(WrappedID3D12GraphicsCommandList *replayList, ResourceId cmd,
                             BakedCmdListInfo &cmdListInfo);

  // this is a list of uint64_t file offset -> uint32_t EIDs of where each
  // action is used. E.g. the action at offset 873954 is EID 50. If a
  // command list is executed more than once, there may be more than
  // one entry here - the action will be aliased among several EIDs, with
  // the first one being the 'primary'
  struct ActionUse
  {
    ActionUse(uint64_t offs, uint32_t eid, ResourceId cmd = ResourceId(), uint32_t rel = 0)
        : fileOffset(offs), cmdList(cmd), eventId(eid), relativeEID(rel)
    {
    }
    uint64_t fileOffset;
    ResourceId cmdList;
    uint32_t eventId;
    uint32_t relativeEID;
    bool operator<(const ActionUse &o) const
    {
      if(fileOffset != o.fileOffset)
        return fileOffset < o.fileOffset;
      return eventId < o.eventId;
    }
  };
  rdcarray<ActionUse> m_ActionUses;

  rdcarray<DebugMessage> m_EventMessages;

  std::map<ResourceId, ID3D12GraphicsCommandListX *> m_RerecordCmds;
  rdcarray<ID3D12GraphicsCommandListX *> m_RerecordCmdList;

  rdcarray<APIEvent> m_Events;
  D3D12EventNode m_LoadingEventNode;
  rdcarray<D3D12EventNode> m_EventNodes;
  bool m_AddedEventNode;

  uint64_t m_CurChunkOffset;
  SDChunkMetaData m_ChunkMetadata;
  uint32_t m_RootEventID;
  uint32_t m_FirstEventID, m_LastEventID;
  D3D12Chunk m_LastChunk;

  ResourceId m_LastPresentedImage;

  uint64_t m_TimeBase = 0;
  double m_TimeFrequency = 1.0f;
  SDFile *m_StructuredFile;

  rdcarray<PatchedRayDispatch> m_RayDispatches;

  std::map<ResourceId, rdcarray<EventUsage>> m_ResourceUses;

  struct IndirectReplayData
  {
    ID3D12CommandSignature *commandSig = NULL;
    ID3D12Resource *argsBuffer = NULL;
    UINT64 argsOffset = 0;
  } m_IndirectData;

  void GetIndirectBuffer(size_t size, ID3D12Resource **buf, uint64_t *offs);

  // util function to handle fetching the right eventId, calling any
  // aliases then calling PreDraw/PreDispatch.
  uint32_t HandlePreCallback(ID3D12GraphicsCommandListX *list,
                             ActionFlags type = ActionFlags::Drawcall, uint32_t multiDrawOffset = 0);

  bool InRerecordRange(ResourceId cmdid);
  bool HasRerecordCmdList(ResourceId cmdid);
  bool IsPartialCmdList(ResourceId cmdid);
  ID3D12GraphicsCommandListX *RerecordCmdList(ResourceId cmdid,
                                              PartialReplayIndex partialType = ePartialNum);

  void AddAction(const ActionDescription &a);
  void AddEvent();

  D3D12EventNode &GetLastEventNode()
  {
    rdcarray<D3D12EventNode> &eventNodes =
        (m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].eventNodes
                                         : m_EventNodes);
    return eventNodes.back();
  }
  D3D12EventNode &GetLoadingEventNode() { return m_LoadingEventNode; }

  void BakeEventNodes(ActionDescription &rootAction);

  void AddUsage(const D3D12RenderState &state, D3D12EventNode &eventNode);

  void AddUsageForBindInRootSig(const D3D12RenderState &state, D3D12EventNode &eventNode,
                                const D3D12RenderState::RootSignature *rootsig,
                                D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t space, uint32_t bind,
                                uint32_t rangeSize);

  void AddResourceUsage(D3D12EventNode &eventNode, ResourceId id, ResourceUsage usage)
  {
    if(id == ResourceId())
      return;

    eventNode.AddResourceUsage(id, usage);
  }
  void AddEventResourceUsage(ResourceId id, ResourceUsage usage)
  {
    m_LoadingEventNode.AddResourceUsage(id, usage);
  }
};
