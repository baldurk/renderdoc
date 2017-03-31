/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "api/replay/renderdoc_replay.h"
#include "common/common.h"
#include "d3d12_common.h"
#include "d3d12_state.h"

struct D3D12DrawcallTreeNode
{
  D3D12DrawcallTreeNode() {}
  explicit D3D12DrawcallTreeNode(const DrawcallDescription &d) : draw(d) {}
  DrawcallDescription draw;
  vector<D3D12DrawcallTreeNode> children;

  vector<pair<ResourceId, EventUsage> > resourceUsage;

  vector<ResourceId> executedCmds;

  D3D12DrawcallTreeNode &operator=(const DrawcallDescription &d)
  {
    *this = D3D12DrawcallTreeNode(d);
    return *this;
  }

  void InsertAndUpdateIDs(const D3D12DrawcallTreeNode &child, uint32_t baseEventID,
                          uint32_t baseDrawID)
  {
    for(size_t i = 0; i < child.resourceUsage.size(); i++)
    {
      resourceUsage.push_back(child.resourceUsage[i]);
      resourceUsage.back().second.eventID += baseEventID;
    }

    for(size_t i = 0; i < child.children.size(); i++)
    {
      children.push_back(child.children[i]);
      children.back().draw.eventID += baseEventID;
      children.back().draw.drawcallID += baseDrawID;

      for(int32_t e = 0; e < children.back().draw.events.count; e++)
        children.back().draw.events[e].eventID += baseEventID;
    }
  }

  vector<DrawcallDescription> Bake()
  {
    vector<DrawcallDescription> ret;
    if(children.empty())
      return ret;

    ret.resize(children.size());
    for(size_t i = 0; i < children.size(); i++)
    {
      ret[i] = children[i].draw;
      ret[i].children = children[i].Bake();
    }

    return ret;
  }
};

struct D3D12DrawcallCallback
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
  virtual void PreDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd) = 0;
  virtual bool PostDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd) = 0;
  virtual void PostRedraw(uint32_t eid, ID3D12GraphicsCommandList *cmd) = 0;

  // same principle as above, but for dispatch calls
  virtual void PreDispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) = 0;
  virtual bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) = 0;
  virtual void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) = 0;

  // should we re-record all command lists? this needs to be true if the range
  // being replayed is larger than one command list (which usually means the
  // whole frame).
  virtual bool RecordAllCmds() = 0;

  // if a command list is recorded once and submitted N > 1 times, then the same
  // drawcall will have several EIDs that refer to it. We'll only do the full
  // callbacks above for the first EID, then call this function for the others
  // to indicate that they are the same.
  virtual void AliasEvent(uint32_t primary, uint32_t alias) = 0;
};

struct BakedCmdListInfo
{
  void BakeFrom(ResourceId parentID, BakedCmdListInfo &parent)
  {
    draw = parent.draw;
    curEvents = parent.curEvents;
    debugMessages = parent.debugMessages;
    eventCount = parent.curEventID;
    drawCount = parent.drawCount;
    crackedLists.swap(parent.crackedLists);
    executeEvents.swap(parent.executeEvents);

    parentList = parentID;

    curEventID = 0;

    parent.draw = NULL;
    parent.curEventID = 0;
    parent.eventCount = 0;
    parent.drawCount = 0;
    parent.curEvents.clear();
    parent.debugMessages.clear();
  }

  void ShiftForRemoved(uint32_t shiftDrawID, uint32_t shiftEID, size_t idx);

  struct ExecuteData
  {
    ExecuteData()
        : baseEvent(0),
          lastEvent(0),
          patched(false),
          argBuf(NULL),
          countBuf(NULL),
          argOffs(0),
          countOffs(0),
          maxCount(0),
          realCount(0)
    {
    }

    uint32_t baseEvent;
    uint32_t lastEvent;
    bool patched;
    ID3D12Resource *argBuf;
    ID3D12Resource *countBuf;
    uint64_t argOffs;
    uint64_t countOffs;
    ResourceId sig;
    UINT maxCount;
    UINT realCount;
  };

  vector<ID3D12GraphicsCommandList *> crackedLists;
  vector<ExecuteData> executeEvents;

  vector<APIEvent> curEvents;
  vector<DebugMessage> debugMessages;
  std::list<D3D12DrawcallTreeNode *> drawStack;

  vector<pair<ResourceId, EventUsage> > resourceUsage;

  ResourceId allocator;
  D3D12_COMMAND_LIST_TYPE type;
  UINT nodeMask;
  D3D12RenderState state;

  vector<D3D12_RESOURCE_BARRIER> barriers;

  ResourceId parentList;

  D3D12DrawcallTreeNode *draw;    // the root draw to copy from when submitting
  uint32_t eventCount;            // how many events are in this cmd list, for quick skipping
  uint32_t curEventID;            // current event ID while reading or executing
  uint32_t drawCount;             // similar to above
};

class WrappedID3D12Device;

struct D3D12CommandData
{
  D3D12CommandData();

  WrappedID3D12Device *m_pDevice;
  Serialiser *m_pSerialiser;

  D3D12DrawcallCallback *m_DrawcallCallback;

  ResourceId m_LastCmdListID;

  map<ResourceId, ID3D12CommandAllocator *> m_CrackedAllocators;

  vector<ID3D12Resource *> m_IndirectBuffers;
  static const uint64_t m_IndirectSize = 4 * 1024 * 1024;
  uint64_t m_IndirectOffset;

  map<ResourceId, BakedCmdListInfo> m_BakedCmdListInfo;

  D3D12RenderState m_RenderState;

  enum PartialReplayIndex
  {
    Primary,
    Secondary,
    ePartialNum
  };

  struct PartialReplayData
  {
    PartialReplayData() { Reset(); }
    void Reset()
    {
      resultPartialCmdList = NULL;
      outsideCmdList = NULL;
      partialParent = ResourceId();
      baseEvent = 0;
    }

    // if we're doing a partial replay, by definition only one command
    // list will be partial at any one time. While replaying through
    // the command list chunks, the partial command list will be
    // created as a temporary new command list and when it comes to
    // the queue that should execute it, it can execute this instead.
    ID3D12GraphicsCommandList *resultPartialCmdList;

    // if we're replaying just a single draw or a particular command
    // list subsection of command events, we don't go through the
    // whole original command lists to set up the partial replay,
    // so we just set this command list
    ID3D12GraphicsCommandList *outsideCmdList;

    // this records where in the frame a command list was executed,
    // so that we know if our replay range ends in one of these ranges
    // we need to construct a partial command list for future
    // replaying. Note that we always have the complete command list
    // around - it's the bakeID itself.
    // Since we only ever record a bakeID once the key is unique - note
    // that the same command list could be reset multiple times
    // a frame, so the parent command list ID (the one recorded in
    // CmdList chunks) is NOT unique.
    // However, a single baked command list can be executed multiple
    // times - so we have to have a list of base events
    // Map from bakeID -> vector<baseEventID>
    map<ResourceId, vector<uint32_t> > cmdListExecs;

    // This is just the ResourceId of the original parent command list
    // and it's baked id.
    // If we are in the middle of a partial replay - allows fast checking
    // in all CmdList chunks, with the iteration through the above list
    // only in Reset.
    // partialParent gets reset to ResourceId() in the Close so that
    // other baked command lists from the same parent don't pick it up
    // Also reset each overall replay
    ResourceId partialParent;

    // If a partial replay is detected, this records the base of the
    // range. This both allows easily and uniquely identifying it in the
    // executecmdlists, but also allows the recording to 'rebase' the
    // last event ID by subtracting this, to know how far to record
    uint32_t baseEvent;
  } m_Partial[ePartialNum];

  void InsertDrawsAndRefreshIDs(ResourceId cmd, vector<D3D12DrawcallTreeNode> &cmdBufNodes);

  // this is a list of uint64_t file offset -> uint32_t EIDs of where each
  // drawcall is used. E.g. the drawcall at offset 873954 is EID 50. If a
  // command list is executed more than once, there may be more than
  // one entry here - the drawcall will be aliased among several EIDs, with
  // the first one being the 'primary'
  struct DrawcallUse
  {
    DrawcallUse(uint64_t offs, uint32_t eid, ResourceId cmd = ResourceId(), uint32_t rel = 0)
        : fileOffset(offs), cmdList(cmd), eventID(eid), relativeEID(rel)
    {
    }
    uint64_t fileOffset;
    ResourceId cmdList;
    uint32_t eventID;
    uint32_t relativeEID;
    bool operator<(const DrawcallUse &o) const
    {
      if(fileOffset != o.fileOffset)
        return fileOffset < o.fileOffset;
      return eventID < o.eventID;
    }
  };
  vector<DrawcallUse> m_DrawcallUses;

  vector<DebugMessage> m_EventMessages;

  map<ResourceId, ID3D12GraphicsCommandList *> m_RerecordCmds;

  bool m_AddedDrawcall;

  vector<APIEvent> m_RootEvents, m_Events;

  uint64_t m_CurChunkOffset;

  uint32_t m_RootEventID, m_RootDrawcallID;
  uint32_t m_FirstEventID, m_LastEventID;

  map<ResourceId, vector<EventUsage> > m_ResourceUses;

  D3D12DrawcallTreeNode m_ParentDrawcall;

  std::list<D3D12DrawcallTreeNode *> m_RootDrawcallStack;

  std::list<D3D12DrawcallTreeNode *> &GetDrawcallStack()
  {
    if(m_LastCmdListID != ResourceId())
      return m_BakedCmdListInfo[m_LastCmdListID].drawStack;

    return m_RootDrawcallStack;
  }

  void GetIndirectBuffer(size_t size, ID3D12Resource **buf, uint64_t *offs);

  // util function to handle fetching the right eventID, calling any
  // aliases then calling PreDraw/PreDispatch.
  uint32_t HandlePreCallback(ID3D12GraphicsCommandList *list, bool dispatch = false,
                             uint32_t multiDrawOffset = 0);

  bool ShouldRerecordCmd(ResourceId cmdid);
  bool InRerecordRange(ResourceId cmdid);
  ID3D12GraphicsCommandList *RerecordCmdList(ResourceId cmdid,
                                             PartialReplayIndex partialType = ePartialNum);

  void AddDrawcall(const DrawcallDescription &d, bool hasEvents, bool addUsage = true);
  void AddEvent(string description);
  void AddUsage(D3D12DrawcallTreeNode &drawNode);
  void AddUsage(D3D12DrawcallTreeNode &drawNode, ResourceId id, uint32_t EID, ResourceUsage usage);
};
