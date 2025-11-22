/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2025 Baldur Karlsson
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

#include "dxil_controlflow.h"
#include "common/formatting.h"
#include "core/settings.h"

RDOC_EXTERN_CONFIG(bool, D3D12_DXILShaderDebugger_Logging);

/*

Inputs are links of blocks : from -> to (can be forwards or backwards links)
Output is a list of uniform control flow blocks which all possible flows go through (not diverged)
and are not in a loop

The algorithm is:

1. Setup
  * Compute all possible known blocks.
  * For each block generate a list of "to" blocks from the input links
  * Any block without links in the input are set to link to the end sentinel (PATH_END)

2. Find DivergentBlocks
  * defined by blocks with more than one exit link

3. Find Uniform Blocks
  * Generate a list of path indexes for each block in the paths
  * Generate a list of loop blocks which are blocks which appear in any path starting from the block
  * Generate a list of all paths blocks which are blocks which appear in all possible paths
    * all paths includes walking any paths linked at the end node of the path being walked
  * uniform blocks are defined to be non-loop blocks which are in all paths

4. Find potential convergent blocks
  * defined to be blocks with more than one link into it and blocks which are directly connected
  to divergent blocks (to handle loop convergence)

5. Find ConvergentBlocks
  * For each divergent block find its convergent block
  * defined to be the first block which is in all possible paths that start from the divergent block
  * this is similar to uniform blocks find convergent blocks starting from the block zero
  * the convergent blocks can be thought of as a local uniform block
  * where local is defined by the graph/paths which contain the divergent block

6. Find Partial ConvergentBlock
  * For each divergent block find its partial convergent blocks in paths before its convergent block
  * defined to be blocks which are in more than one path that start from the divergent block
  * this is similar to finding convergent blocks
  * prune partial convergence blocks from existing divergent block entries
    * if an existing divergent block includes this partial convergence block
    * and that divergent block is directly connected to this divergent block by a single path
    * then the previous entry is superceded (removed)

*/

namespace DXIL
{
void OutputGraph(const char *const name, const ControlFlow *graph)
{
  rdcstr fname = StringFormat::Fmt("%s.txt", name);
  FILE *f = FileIO::fopen(fname.c_str(), FileIO::WriteText);
  rdcstr output = GenerateGraph(name, graph);
  fprintf(f, "%s", output.c_str());
  FileIO::fclose(f);
}

rdcstr GenerateGraph(const char *const name, const ControlFlow *graph)
{
  BlockArray divergentBlocks = graph->GetDivergentBlocks();
  rdcarray<ConvergentBlockData> convergentBlockDatas = graph->GetConvergentBlocks();
  rdcarray<PartialConvergentBlockData> partialConvergentBlockDatas =
      graph->GetPartialConvergentBlocks();
  BlockArray convergentBlocks;
  for(const ConvergentBlockData &data : convergentBlockDatas)
    convergentBlocks.push_back(data.second);
  BlockArray partialConvergentBlocks;
  for(const PartialConvergentBlockData &data : partialConvergentBlockDatas)
  {
    for(const uint32_t &to : data.second)
      partialConvergentBlocks.push_back(to);
  }

  rdcstr output;
  output += StringFormat::Fmt("digraph %s {\n", name);
  for(uint32_t from : graph->m_Blocks)
  {
    output += StringFormat::Fmt("%u", from);
    if(divergentBlocks.contains(from))
      output += StringFormat::Fmt(" [shape=diamond color=red]");
    else if(convergentBlocks.contains(from))
      output += StringFormat::Fmt(" [shape=rectangle color=blue]");
    else if(partialConvergentBlocks.contains(from))
      output += StringFormat::Fmt(" [shape=Mrecord color=green]");
    output += ";\n";

    const ControlFlow::Node *node = graph->GetNode(from);
    for(const ControlFlow::Node *child : node->children)
      output += StringFormat::Fmt("%u -> %u;\n", from, child->blockId);
  }
  for(const ConvergentBlockData &data : convergentBlockDatas)
  {
    output += StringFormat::Fmt("%u -> %u [style=dashed color=blue];\n", data.first, data.second);
  }
  for(const PartialConvergentBlockData &data : partialConvergentBlockDatas)
  {
    for(const uint32_t &to : data.second)
    {
      output += StringFormat::Fmt("%u -> %u [style=dashed color=green];\n", data.first, to);
    }
  }
  output += "}\n";

  return output;
}

bool ControlFlow::BlockInAllPaths(uint32_t from, uint32_t to) const
{
  uint32_t fromIdx = m_BlockIDsToIDx[from];
  if(fromIdx == 0xFFFFFFFF)
    return false;
  uint32_t toIdx = m_BlockIDsToIDx[to];
  if(toIdx == 0xFFFFFFFF)
    return false;

  memset(m_TracedNodes.data(), 0, m_TracedNodes.size() * sizeof(uint8_t));

  const Node *fromNode = m_Nodes.data() + fromIdx;
  rdcarray<const Node *> nodesToCheck;
  nodesToCheck.push_back(fromNode);
  while(!nodesToCheck.empty())
  {
    const Node *currentNode = nodesToCheck.back();
    if(currentNode->children.empty())
      return false;

    nodesToCheck.pop_back();
    m_TracedNodes[currentNode->idx] = 1;

    if(m_Connections[currentNode->blockId][to] == ConnectionState::NotConnected)
      return false;

    for(const Node *child : currentNode->children)
    {
      if(child->blockId == to)
      {
        m_Connections[from][to] = ConnectionState::Connected;
        continue;
      }

      if(!m_TracedNodes[child->idx])
        nodesToCheck.push_back(child);
    }
  }
  return true;
}

bool ControlFlow::BlockInMultiplePaths(uint32_t from, uint32_t to) const
{
  uint32_t fromIdx = m_BlockIDsToIDx[from];
  if(fromIdx == 0xFFFFFFFF)
    return false;
  uint32_t toIdx = m_BlockIDsToIDx[to];
  if(toIdx == 0xFFFFFFFF)
    return false;

  memset(m_TracedNodes.data(), 0, m_TracedNodes.size() * sizeof(uint8_t));
  uint32_t countPaths = 0;

  const Node *fromNode = m_Nodes.data() + fromIdx;
  rdcarray<const Node *> nodesToCheck;
  nodesToCheck.push_back(fromNode);
  while(!nodesToCheck.empty())
  {
    const Node *currentNode = nodesToCheck.back();
    nodesToCheck.pop_back();
    m_TracedNodes[currentNode->idx] = 1;

    if(m_Connections[currentNode->blockId][to] == ConnectionState::NotConnected)
      continue;

    for(const Node *child : currentNode->children)
    {
      if(child->blockId == to)
      {
        m_Connections[from][to] = ConnectionState::Connected;
        if(countPaths > 0)
          return true;
        ++countPaths;
        continue;
      }

      if(!m_TracedNodes[child->idx])
        nodesToCheck.push_back(child);
    }
  }
  return false;
}

int32_t ControlFlow::BlockInAnyPath(uint32_t from, uint32_t to) const
{
  uint32_t fromIdx = m_BlockIDsToIDx[from];
  if(fromIdx == 0xFFFFFFFF)
    return false;
  uint32_t toIdx = m_BlockIDsToIDx[to];
  if(toIdx == 0xFFFFFFFF)
    return false;

  rdcarray<int32_t> tracedNodes;
  tracedNodes.resize(m_Nodes.size());
  for(size_t i = 0; i < tracedNodes.size(); ++i)
    tracedNodes[i] = INT_MAX;

  struct NodeData
  {
    const Node *node;
    int32_t steps;
  };
  const Node *fromNode = m_Nodes.data() + fromIdx;
  rdcarray<NodeData> nodesToCheck;
  NodeData startData = {fromNode, 0};
  nodesToCheck.push_back(startData);
  while(!nodesToCheck.empty())
  {
    NodeData nodeData = nodesToCheck.back();
    nodesToCheck.pop_back();
    const Node *currentNode = nodeData.node;
    int32_t steps = nodeData.steps;
    if(tracedNodes[currentNode->idx] < steps)
      continue;

    if(m_Connections[currentNode->blockId][to] == ConnectionState::NotConnected)
      continue;

    tracedNodes[currentNode->idx] = steps;
    steps += 1;

    for(const Node *child : currentNode->children)
    {
      if(child->blockId == to)
        return steps;

      if(steps < tracedNodes[child->idx])
      {
        NodeData childData = {child, steps};
        nodesToCheck.push_back(childData);
      }
    }
  }
  return -1;
}

bool ControlFlow::AllPathsContainBlock(uint32_t from, uint32_t to, uint32_t mustInclude) const
{
  uint32_t fromIdx = m_BlockIDsToIDx[from];
  if(fromIdx == 0xFFFFFFFF)
    return false;
  uint32_t toIdx = m_BlockIDsToIDx[to];
  if(toIdx == 0xFFFFFFFF)
    return false;

  memset(m_TracedNodes.data(), 0, m_TracedNodes.size() * sizeof(uint8_t));

  const Node *fromNode = m_Nodes.data() + fromIdx;
  struct NodeData
  {
    const Node *node;
    bool foundMustInclude;
  };

  rdcarray<NodeData> nodesToCheck;
  nodesToCheck.emplace_back(NodeData({fromNode, false}));
  bool foundEnd = false;
  while(!nodesToCheck.empty())
  {
    NodeData &nodeData = nodesToCheck.back();
    const Node *currentNode = nodeData.node;
    bool foundMustInclude = nodeData.foundMustInclude;

    nodesToCheck.pop_back();
    m_TracedNodes[currentNode->idx] = 1;

    if(m_Connections[currentNode->blockId][to] == ConnectionState::NotConnected)
      continue;

    for(const Node *child : currentNode->children)
    {
      if(child->blockId == to)
      {
        m_Connections[from][to] = ConnectionState::Connected;
        if(!foundMustInclude)
          return false;
        foundEnd = true;
        continue;
      }

      if(!m_TracedNodes[child->idx])
      {
        bool newMustInclude = foundMustInclude || (child->blockId == mustInclude);
        nodesToCheck.emplace_back(NodeData({child, newMustInclude}));
      }
    }
  }
  return foundEnd;
}

bool ControlFlow::AnyPath(uint32_t from, uint32_t to) const
{
  uint32_t fromIdx = m_BlockIDsToIDx[from];
  if(fromIdx == 0xFFFFFFFF)
    return false;
  uint32_t toIdx = m_BlockIDsToIDx[to];
  if(toIdx == 0xFFFFFFFF)
    return false;

  memset(m_TracedNodes.data(), 0, m_TracedNodes.size() * sizeof(uint8_t));

  const Node *fromNode = m_Nodes.data() + fromIdx;
  rdcarray<const Node *> nodesToCheck;
  nodesToCheck.push_back(fromNode);
  while(!nodesToCheck.empty())
  {
    const Node *currentNode = nodesToCheck.back();
    nodesToCheck.pop_back();
    m_TracedNodes[currentNode->idx] = 1;

    if(m_Connections[currentNode->blockId][to] == ConnectionState::Connected)
      return true;
    if(m_Connections[currentNode->blockId][to] == ConnectionState::NotConnected)
      continue;

    for(const Node *child : currentNode->children)
    {
      if(child->blockId == to)
        return true;

      if(!m_TracedNodes[child->idx])
        nodesToCheck.push_back(child);
    }
  }
  return false;
}

void ControlFlow::Construct(const rdcarray<rdcpair<uint32_t, uint32_t>> &links)
{
  m_Nodes.clear();
  m_BlockIDsToIDx.clear();
  m_TracedNodes.clear();

  m_Blocks.clear();

  m_UniformBlocks.clear();
  m_LoopBlocks.clear();
  m_DivergentBlocks.clear();
  m_ConvergentBlocks.clear();
  m_PartialConvergentBlocks.clear();

  // 1. Setup
  // Compute all possible known blocks
  uint32_t maxBlockIndex = 0;
  for(const auto &link : links)
  {
    uint32_t from = link.first;
    uint32_t to = link.second;
    m_Blocks.insert(from);
    m_Blocks.insert(to);
    maxBlockIndex = RDCMAX(maxBlockIndex, from);
    maxBlockIndex = RDCMAX(maxBlockIndex, to);
  }
  PATH_END = links.empty() ? 0 : maxBlockIndex + 1;
  maxBlockIndex = PATH_END + 1;

  // Create Nodes to represent the blocks
  size_t countNodes = m_Blocks.size();
  m_Nodes.resize(countNodes);
  m_BlockIDsToIDx.resize(maxBlockIndex);
  memset(m_BlockIDsToIDx.data(), 0xFF, m_BlockIDsToIDx.size() * sizeof(uint32_t));

  uint32_t nodeIdx = 0;
  for(uint32_t b : m_Blocks)
  {
    uint32_t idx = nodeIdx;
    m_BlockIDsToIDx[b] = idx;
    Node &node = m_Nodes[idx];
    node.blockId = b;
    node.idx = idx;
    node.parents.clear();
    node.children.clear();
    ++nodeIdx;
  }

  // add the PATH_END node
  m_BlockIDsToIDx[PATH_END] = nodeIdx;
  uint32_t pathEndIdx = m_BlockIDsToIDx[PATH_END];
  {
    Node node;
    node.blockId = PATH_END;
    node.idx = pathEndIdx;
    node.parents.clear();
    node.children.clear();
    m_Nodes.push_back(node);
  }

  m_TracedNodes.resize(m_Nodes.size());

  for(const auto &link : links)
  {
    uint32_t from = link.first;
    uint32_t to = link.second;
    uint32_t fromIdx = m_BlockIDsToIDx[from];
    uint32_t toIdx = m_BlockIDsToIDx[to];
    m_Nodes[fromIdx].children.push_back(m_Nodes.data() + toIdx);
    m_Nodes[toIdx].parents.push_back(m_Nodes.data() + fromIdx);
  }

  // Any node without children are set to link to the end sentinel (PATH_END)
  for(Node &n : m_Nodes)
  {
    if(n.idx == pathEndIdx)
      continue;
    if(n.children.empty())
    {
      n.children.push_back(m_Nodes.data() + pathEndIdx);
      m_Nodes[pathEndIdx].parents.push_back(m_Nodes.data() + n.idx);
    }
  }

  // 2. Find DivergentBlocks
  //  * defined by blocks with more than one exit link
  for(const Node &node : m_Nodes)
  {
    if(node.children.size() > 1)
      m_DivergentBlocks.push_back(node.blockId);
  }

  // 3. Find Uniform Blocks

  // Generate the connections 2D map for quick lookup of forward connections
  // IsBlock B in any path ahead of Block A
  m_Connections.resize(maxBlockIndex);
  for(uint32_t from = 0; from < maxBlockIndex; ++from)
  {
    m_Connections[from].resize(maxBlockIndex);
    for(uint32_t to = 0; to < maxBlockIndex; ++to)
      m_Connections[from][to] = ConnectionState::Unknown;
  }

  // A loop block is defined by any block which appears in any path (including loops) starting from the block
  for(uint32_t block : m_Blocks)
  {
    if(IsConnected(block, block))
      m_LoopBlocks.push_back(block);
  }

  // An all paths block is defined by any block which appears in all paths
  // all paths includes walking any paths linked at the end node of the path being walked
  uint32_t globalDivergenceStart = 0;
  if(!m_Blocks.empty())
  {
    m_UniformBlocks.push_back(globalDivergenceStart);

    // We want all uniform blocks connected to the global start block
    // Not just the first convergence points
    for(uint32_t block : m_Blocks)
    {
      if(block == globalDivergenceStart)
        continue;
      // Ignore loop blocks
      if(m_LoopBlocks.contains(block))
        continue;
      // Ignore blocks not connected to the divergent block
      if(!IsConnected(globalDivergenceStart, block))
        continue;

      bool uniform = BlockInAllPaths(globalDivergenceStart, block);
      if(uniform)
        m_UniformBlocks.push_back(block);
    }
  }

  // 4. Find potential convergent blocks
  // * defined to be blocks with more than one link into it and blocks which are directly
  // connected to divergent blocks (to handle loop convergence)
  BlockArray potentialConvergentBlocks;
  for(uint32_t start : m_DivergentBlocks)
  {
    const Node *startNode = GetNode(start);
    for(const Node *child : startNode->children)
    {
      if(!potentialConvergentBlocks.contains(child->blockId))
        potentialConvergentBlocks.push_back(child->blockId);
    }
  }
  // potential partial convergent blocks
  // * defined to be blocks with more than one link into it
  BlockArray potentialPartialConvergentBlocks;
  for(const Node &node : m_Nodes)
  {
    if(node.parents.size() > 1)
    {
      uint32_t block = node.blockId;
      if(!potentialPartialConvergentBlocks.contains(block))
        potentialPartialConvergentBlocks.push_back(block);
      if(!potentialConvergentBlocks.contains(block))
        potentialConvergentBlocks.push_back(block);
    }
  }

  // 5. Find ConvergentBlocks
  //  * For each divergent block find its convergent block
  //  * defined to be the first block which is in all possible paths that start from the divergent block
  //  * this is similar to uniform blocks which find convergent blocks starting from the block zero
  //  * the convergent blocks can be thought of as a local uniform block
  //  * where local is defined by the graph/paths which contain the divergent block

  m_ConvergentBlocks.clear();
  BlockArray allConvergentBlocks;
  for(uint32_t start : m_DivergentBlocks)
  {
    BlockArray localUniformBlocks;
    for(uint32_t block : potentialConvergentBlocks)
    {
      if(block == start)
        continue;
      if(m_Connections[start][block] == ConnectionState::NotConnected)
        continue;

      bool uniform = BlockInAllPaths(start, block);
      if(uniform)
        localUniformBlocks.push_back(block);
    }
    if(localUniformBlocks.empty())
      RDCERR("Failed to find any locally uniform blocks for divergent block %d", start);

    uint32_t convergentBlock = start;
    // Take the first uniform block which is in all paths
    for(uint32_t blockA : localUniformBlocks)
    {
      uint32_t countConnected = 0;
      bool first = true;
      for(uint32_t blockB : localUniformBlocks)
      {
        if(blockA == blockB)
          continue;
        if(!IsConnected(blockA, blockB))
        {
          first = false;
          break;
        }
        ++countConnected;
      }
      if(first)
      {
        RDCASSERTEQUAL(countConnected, localUniformBlocks.size() - 1);
        convergentBlock = blockA;
        break;
      }
    }
    if(start != convergentBlock)
    {
      m_ConvergentBlocks.push_back({start, convergentBlock});
      allConvergentBlocks.push_back(convergentBlock);
    }
    else
    {
      RDCERR("Failed to find convergent block for divergent block %d", start);
    }
  }

  // 6. Find Partial ConvergentBlock
  //  * For each divergent block find its partial convergent blocks in paths before its convergent block
  //  * defined to be blocks which are in more than one path that start from the divergent block
  //  * this is similar to finding convergent blocks
  //  * prune partial convergence blocks from existing divergent block entries
  //    * if an existing divergent block includes this partial convergence block
  //    * and that divergent block is directly connected to this divergent block by a single path
  //    * then the previous entry is superceded (removed)

  rdcarray<PartialConvergentBlockData> localPartialConvergentBlocks;
  for(uint32_t start : m_DivergentBlocks)
  {
    // Find the convergent block for this divergent block
    uint32_t convergentBlock = start;
    for(const ConvergentBlockData &data : m_ConvergentBlocks)
    {
      if(data.first == start)
        convergentBlock = data.second;
    }
    // Find the partial ConvergentBlock for the divergent block
    BlockArray partialConvergentBlocks;
    BlockArray localPotentialPartialConvergentBlocks;
    for(uint32_t block : potentialPartialConvergentBlocks)
    {
      if(block == start)
        continue;
      // Ignore blocks not connected to the divergent block
      if(!IsConnected(start, block))
        continue;

      // Ignore blocks which are already full convergent blocks
      if(allConvergentBlocks.contains(block))
        continue;

      localPotentialPartialConvergentBlocks.push_back(block);
    }
    for(uint32_t block : localPotentialPartialConvergentBlocks)
    {
      // Only consider blocks which are directly connected to the full convergence block
      if(!IsConnected(block, convergentBlock))
        continue;

      // Looking for blocks which are in more than one path
      if(BlockInMultiplePaths(start, block))
      {
        partialConvergentBlocks.push_back(block);
        // This partial convergence might supercede an earlier one
        //  Prune partial convergence blocks from existing divergent block entries
        for(PartialConvergentBlockData &data : localPartialConvergentBlocks)
        {
          if(data.second.contains(block))
          {
            // If ALL paths from the earlier partial convergence path go thru this start then the
            // earlier start is superseded
            const uint32_t pathStart = data.first;
            const uint32_t pathEnd = block;
            if(AllPathsContainBlock(pathStart, pathEnd, start))
              data.second.removeOne(block);
          }
        }
      }
    }
    if(!partialConvergentBlocks.empty())
      localPartialConvergentBlocks.push_back({start, partialConvergentBlocks});
  }
  // Only keep non-empty partial convergent block datasets
  for(PartialConvergentBlockData &data : localPartialConvergentBlocks)
  {
    if(data.second.empty())
      continue;
    // sort the partial convergence points from least to most inter-connections
    rdcarray<rdcpair<uint32_t, uint32_t>> sortInfos;
    BlockArray &unsortedPartialBlocks = data.second;
    sortInfos.resize(unsortedPartialBlocks.size());
    for(uint32_t i = 0; i < sortInfos.size(); ++i)
      sortInfos[i] = {i, 0};

    for(uint32_t i = 0; i < unsortedPartialBlocks.size(); ++i)
    {
      uint32_t from = unsortedPartialBlocks[i];
      // Count the connections between "from" and all the other partial blocks
      for(uint32_t j = 0; j < unsortedPartialBlocks.size(); ++j)
      {
        if(i == j)
          continue;
        uint32_t to = unsortedPartialBlocks[j];
        if(IsConnected(from, to))
          sortInfos[i].second++;
      }
    }
    std::sort(sortInfos.begin(), sortInfos.end(),
              [](const rdcpair<uint32_t, uint32_t> &a, const rdcpair<uint32_t, uint32_t> &b) {
                return a.second < b.second;
              });

    BlockArray sortedPartialBlocks;
    uint32_t prevCount = 0;
    for(const rdcpair<uint32_t, uint32_t> &sortInfo : sortInfos)
    {
      uint32_t index = sortInfo.first;
      uint32_t block = unsortedPartialBlocks[index];
      sortedPartialBlocks.push_back(block);
      RDCASSERT(sortInfo.second >= prevCount);
      prevCount = sortInfo.second;
    }

    m_PartialConvergentBlocks.push_back({data.first, sortedPartialBlocks});
  }

  if(D3D12_DXILShaderDebugger_Logging())
  {
    RDCLOG("Block Links:");
    for(const Node &node : m_Nodes)
    {
      if(node.children.empty())
        continue;
      uint32_t from = (uint32_t)node.blockId;
      for(const Node *child : node.children)
        RDCLOG("Block:%d->Block:%d", from, child->blockId);
    }

    for(const Node &node : m_Nodes)
    {
      if(node.parents.empty())
        continue;
      uint32_t to = (uint32_t)node.blockId;
      for(const Node *parent : node.parents)
        RDCLOG("Block:%d->Block:%d", parent->blockId, to);
    }

    rdcstr output = "";
    bool needComma = false;
    for(uint32_t block : m_LoopBlocks)
    {
      if(needComma)
        output += ", ";
      output += ToStr(block);
      needComma = true;
    }
    RDCLOG("Blocks in Loops: %s", output.c_str());

    output = "";
    needComma = false;
    for(uint32_t block : m_UniformBlocks)
    {
      if(needComma)
        output += ", ";
      output += ToStr(block);
      needComma = true;
    }
    RDCLOG("Uniform Blocks: %s", output.c_str());

    output = "";
    needComma = false;
    for(uint32_t block : m_DivergentBlocks)
    {
      if(needComma)
        output += ", ";
      output += ToStr(block);
      needComma = true;
    }
    RDCLOG("Divergent Blocks: %s", output.c_str());

    output = "";
    needComma = false;
    for(ConvergentBlockData data : m_ConvergentBlocks)
    {
      if(needComma)
        output += ", ";
      output += StringFormat::Fmt("{ %d -> %d }", data.first, data.second);
      needComma = true;
    }
    RDCLOG("Convergent Blocks: %s", output.c_str());

    output = "";
    for(PartialConvergentBlockData data : m_PartialConvergentBlocks)
    {
      output += "\n";
      output += StringFormat::Fmt("%d -> { ", data.first);
      needComma = false;
      for(uint32_t block : data.second)
      {
        if(needComma)
          output += ", ";
        output += ToStr(block);
        needComma = true;
      }
      output += " }";
    }
    RDCLOG("Partial Convergent Blocks: %s", output.c_str());

    RDCLOG("GraphVis Data:");
    output = GenerateGraph("dxil_cfg", this);
    RDCLOG("%s", output.c_str());
  }
  // OutputGraph("dxil_cfg", this);
}

uint32_t ControlFlow::GetNextUniformBlock(uint32_t from) const
{
  // find the closest uniform block when walking the path starting at the from block
  int32_t minSteps = INT_MAX;
  uint32_t bestBlock = from;
  for(uint32_t uniform : m_UniformBlocks)
  {
    if(uniform == from)
      continue;
    int32_t steps = BlockInAnyPath(from, uniform);
    if(steps != -1)
    {
      if(steps < minSteps)
      {
        minSteps = steps;
        bestBlock = uniform;
      }
    }
  }
  return bestBlock;
}

bool ControlFlow::IsConnected(uint32_t from, uint32_t to) const
{
  if(m_Connections[from][to] == ConnectionState::Unknown)
  {
    if(AnyPath(from, to))
      m_Connections[from][to] = ConnectionState::Connected;
    else
      m_Connections[from][to] = ConnectionState::NotConnected;
  }
  return m_Connections[from][to] == ConnectionState::Connected;
}
};    // namespace DXIL

#if ENABLED(ENABLE_UNIT_TESTS)

#include <limits>
#include "catch/catch.hpp"

using namespace DXIL;

void CheckDivergentBlocks(const rdcarray<ConvergentBlockData> &expectedData,
                          const BlockArray &actualData)
{
  REQUIRE(expectedData.count() == actualData.count());
  for(ConvergentBlockData expected : expectedData)
  {
    bool found = false;
    for(uint32_t actual : actualData)
    {
      if(expected.first == actual)
      {
        found = true;
      }
    }
    REQUIRE(found);
  }
}

void CheckConvergentBlocks(const rdcarray<ConvergentBlockData> &expectedData,
                           const rdcarray<ConvergentBlockData> &actualData)
{
  REQUIRE(expectedData.count() == actualData.count());
  for(ConvergentBlockData expected : expectedData)
  {
    bool found = false;
    for(ConvergentBlockData actual : actualData)
    {
      if(expected.first == actual.first)
      {
        found = true;
        REQUIRE(expected.second == actual.second);
      }
    }
    REQUIRE(found);
  }
}

void CheckPartialConvergentBlocks(const rdcarray<PartialConvergentBlockData> &expectedData,
                                  const rdcarray<PartialConvergentBlockData> &actualData)
{
  REQUIRE(expectedData.count() == actualData.count());
  for(PartialConvergentBlockData expected : expectedData)
  {
    bool found = false;
    for(PartialConvergentBlockData actual : actualData)
    {
      if(expected.first == actual.first)
      {
        found = true;
        REQUIRE(expected.second == actual.second);
      }
    }
    REQUIRE(found);
  }
}

void CheckUniformBlocks(ControlFlow &controlFlow)
{
  const std::unordered_set<uint32_t> &blocks = controlFlow.GetBlocks();
  const BlockArray &uniformBlocks = controlFlow.GetUniformBlocks();
  for(uint32_t block : blocks)
  {
    uint32_t nextUniform = controlFlow.GetNextUniformBlock(block);
    if(nextUniform != block)
      REQUIRE(uniformBlocks.contains(nextUniform));
  }
}

TEST_CASE("DXIL Control Flow", "[dxil][controlflow]")
{
  SECTION("FindUniformBlocks")
  {
    ControlFlow controlFlow;
    BlockArray uniformBlocks;
    BlockArray loopBlocks;
    SECTION("Degenerate Case")
    {
      // Degenerate case
      rdcarray<BlockLink> inputs;
      controlFlow.Construct(inputs);
      uniformBlocks = controlFlow.GetUniformBlocks();
      REQUIRE(0 == uniformBlocks.count());
      loopBlocks = controlFlow.GetLoopBlocks();
      REQUIRE(0 == loopBlocks.count());
      CheckUniformBlocks(controlFlow);
    }
    SECTION("Just Start and End")
    {
      // Only uniform flow is the start and end
      // 0 -> 1
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      controlFlow.Construct(inputs);
      uniformBlocks = controlFlow.GetUniformBlocks();
      REQUIRE(2 == uniformBlocks.count());
      REQUIRE(uniformBlocks.contains(0U));
      REQUIRE(uniformBlocks.contains(1U));
      loopBlocks = controlFlow.GetLoopBlocks();
      REQUIRE(0 == loopBlocks.count());
      CheckUniformBlocks(controlFlow);
    }

    SECTION("Single Uniform Flow")
    {
      // Single uniform flow between start and end
      // 0 -> 1 -> 2 -> 3 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      controlFlow.Construct(inputs);
      uniformBlocks = controlFlow.GetUniformBlocks();
      REQUIRE(5 == uniformBlocks.count());
      REQUIRE(uniformBlocks.contains(0U));
      REQUIRE(uniformBlocks.contains(1U));
      REQUIRE(uniformBlocks.contains(2U));
      REQUIRE(uniformBlocks.contains(3U));
      REQUIRE(uniformBlocks.contains(4U));
      loopBlocks = controlFlow.GetLoopBlocks();
      REQUIRE(0 == loopBlocks.count());
      CheckUniformBlocks(controlFlow);
    }

    SECTION("Simple Branch")
    {
      // Simple branch
      // 0 -> 1
      // 0 -> 2
      // 1 -> 2
      // 2 -> 3
      // 2 -> 4
      // 3 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 4});
      controlFlow.Construct(inputs);
      uniformBlocks = controlFlow.GetUniformBlocks();
      REQUIRE(3 == uniformBlocks.count());
      REQUIRE(uniformBlocks.contains(0U));
      REQUIRE(uniformBlocks.contains(2U));
      REQUIRE(uniformBlocks.contains(4U));
      loopBlocks = controlFlow.GetLoopBlocks();
      REQUIRE(0 == loopBlocks.count());
      CheckUniformBlocks(controlFlow);
    }
    SECTION("Finite Loop1")
    {
      // Finite loop (3 -> 4 -> 5 -> 3)
      // 0 -> 1 -> 3
      // 0 -> 2 -> 3
      // 3 -> 4 -> 5
      // 4 -> 6
      // 5 -> 3
      // 5 -> 6
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 3});
      inputs.push_back({0, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      inputs.push_back({4, 5});
      inputs.push_back({4, 6});
      inputs.push_back({5, 3});
      inputs.push_back({5, 6});
      controlFlow.Construct(inputs);
      uniformBlocks = controlFlow.GetUniformBlocks();
      REQUIRE(2 == uniformBlocks.count());
      REQUIRE(uniformBlocks.contains(0U));
      REQUIRE(uniformBlocks.contains(6U));
      loopBlocks = controlFlow.GetLoopBlocks();
      REQUIRE(3 == loopBlocks.count());
      REQUIRE(loopBlocks.contains(3U));
      REQUIRE(loopBlocks.contains(4U));
      REQUIRE(loopBlocks.contains(5U));
      CheckUniformBlocks(controlFlow);
    }
    SECTION("Finite Loop2")
    {
      // Finite loop (3 -> 4 -> 5 -> 3)
      // 0 -> 1 -> 2
      // 0 -> 2
      // 2 -> 3
      // 3 -> 4 -> 5 -> 6
      // 3 -> 5 -> 3
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({0, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      inputs.push_back({4, 5});
      inputs.push_back({5, 6});
      inputs.push_back({3, 5});
      inputs.push_back({5, 3});
      controlFlow.Construct(inputs);
      uniformBlocks = controlFlow.GetUniformBlocks();
      REQUIRE(3 == uniformBlocks.count());
      REQUIRE(uniformBlocks.contains(0U));
      REQUIRE(uniformBlocks.contains(2U));
      REQUIRE(uniformBlocks.contains(6U));
      loopBlocks = controlFlow.GetLoopBlocks();
      REQUIRE(3 == loopBlocks.count());
      REQUIRE(loopBlocks.contains(3U));
      REQUIRE(loopBlocks.contains(4U));
      REQUIRE(loopBlocks.contains(5U));
      CheckUniformBlocks(controlFlow);
    }

    SECTION("Infinite Loop")
    {
      // Infinite loop which never converges (3 -> 4 -> 3)
      // 0 -> 1 -> 3
      // 0 -> 2 -> 3
      // 3 -> 4
      // 4 -> 3
      // 1 -> 6
      // 2 -> 6
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 3});
      inputs.push_back({0, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      inputs.push_back({4, 3});
      inputs.push_back({1, 6});
      inputs.push_back({2, 6});
      controlFlow.Construct(inputs);
      uniformBlocks = controlFlow.GetUniformBlocks();
      REQUIRE(2 == uniformBlocks.count());
      REQUIRE(uniformBlocks.contains(0U));
      REQUIRE(uniformBlocks.contains(6U));
      loopBlocks = controlFlow.GetLoopBlocks();
      REQUIRE(2 == loopBlocks.count());
      REQUIRE(loopBlocks.contains(3U));
      REQUIRE(loopBlocks.contains(4U));
      CheckUniformBlocks(controlFlow);
    }

    SECTION("Complex Case Two Loops")
    {
      // Complex case with two loops
      // Loop: 7 -> 9 -> 7, 13 -> 15 -> 13
      // 0 -> 1 -> 3
      // 0 -> 2 -> 3
      // 3 -> 4 -> 5
      // 3 -> 5
      // 5 -> 6 -> 7
      // 5 -> 11
      // 7 -> 8 -> 11
      // 7 -> 9 -> 7
      // 9 -> 10 -> 11 -> 12 -> 13 -> 14 -> 17 -> 18 -> 19 -> 20 -> 21 -> 22 -> 23 -> 26
      // 13 -> 15 -> 13
      // 15 -> 16 -> 17 -> 19 -> 21 -> 26
      // 11 -> 17
      // 22 -> 24 -> 25 -> 26
      // 24 -> 26
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({2, 3});
      inputs.push_back({1, 3});
      inputs.push_back({3, 4});
      inputs.push_back({4, 5});
      inputs.push_back({3, 5});
      inputs.push_back({5, 6});
      inputs.push_back({9, 7});
      inputs.push_back({6, 7});
      inputs.push_back({7, 8});
      inputs.push_back({7, 9});
      inputs.push_back({9, 10});
      inputs.push_back({10, 11});
      inputs.push_back({8, 11});
      inputs.push_back({5, 11});
      inputs.push_back({11, 12});
      inputs.push_back({15, 13});
      inputs.push_back({12, 13});
      inputs.push_back({13, 14});
      inputs.push_back({13, 15});
      inputs.push_back({15, 16});
      inputs.push_back({16, 17});
      inputs.push_back({14, 17});
      inputs.push_back({11, 17});
      inputs.push_back({17, 18});
      inputs.push_back({18, 19});
      inputs.push_back({17, 19});
      inputs.push_back({19, 20});
      inputs.push_back({20, 21});
      inputs.push_back({19, 21});
      inputs.push_back({21, 22});
      inputs.push_back({22, 23});
      inputs.push_back({22, 24});
      inputs.push_back({24, 25});
      inputs.push_back({25, 26});
      inputs.push_back({24, 26});
      inputs.push_back({23, 26});
      inputs.push_back({21, 26});
      controlFlow.Construct(inputs);
      uniformBlocks = controlFlow.GetUniformBlocks();
      REQUIRE(8 == uniformBlocks.count());
      REQUIRE(uniformBlocks.contains(0U));
      REQUIRE(uniformBlocks.contains(3U));
      REQUIRE(uniformBlocks.contains(5U));
      REQUIRE(uniformBlocks.contains(11U));
      REQUIRE(uniformBlocks.contains(17U));
      REQUIRE(uniformBlocks.contains(19U));
      REQUIRE(uniformBlocks.contains(21U));
      REQUIRE(uniformBlocks.contains(26U));
      loopBlocks = controlFlow.GetLoopBlocks();
      REQUIRE(4 == loopBlocks.count());
      REQUIRE(loopBlocks.contains(7U));
      REQUIRE(loopBlocks.contains(9U));
      REQUIRE(loopBlocks.contains(13U));
      REQUIRE(loopBlocks.contains(15U));
      CheckUniformBlocks(controlFlow);
    }
    SECTION("Complex Case Multiple Loops")
    {
      // Complex case with multiple loops: 4 -> 5 -> 6 -> 4, 10 -> 11 -> 12 -> 10, 68
      // 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 4
      // 0 -> 2 -> 8 -> 9 -> 10 -> 11 -> 12 -> 10
      // 4 -> 6 -> 7 -> 8 -> 14 -> 15 -> 19 -> 20 -> 24 -> 25 -> 29 -> 31 -> 32 -> 33
      // 10 -> 12 -> 13 -> 14 -> 16 -> 17 -> 19
      // 16 -> 18 -> 19 -> 21 -> 22 -> 23 -> 24 -> 26 -> 27 -> 29 -> 30 -> 33 -> 35 -> 37
      // 22 -> 24
      // 26 -> 28 -> 29

      // 31 -> 33 -> 34 -> 37 -> 39 -> 40 -> 41 -> 42 -> 43 -> 44 -> 45 -> 47 -> 49 -> 51 -> 52 ->
      //   53 -> 54 -> 55 -> 57 -> 58 -> 59 -> 60 -> 61 -> 62 -> 63 -> 64 -> 65 -> 66 -> 68 -> 67
      //   -> 69 -> 70 -> 71 -> 72 -> 73 -> 74 -> 75 -> 76 -> 77 -> 78 -> 79 -> END

      // 35 -> 36 -> 37 -> 38 -> 41 39 -> 41 -> 43 -> 45 -> 46 -> 47 -> 48 -> 49 -> 50 -> 51
      // 51 -> 53 -> 55 -> 56 -> 57 -> 59 -> 61 -> 63 -> 65 -> 69 -> 71 -> 73 -> 75 -> 77 -> 79
      // 68 -> 68
      rdcarray<BlockLink> inputs;
      inputs.push_back({8, 9});
      inputs.push_back({8, 14});
      inputs.push_back({64, 65});
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 8});
      inputs.push_back({6, 4});
      inputs.push_back({6, 7});
      inputs.push_back({3, 4});
      inputs.push_back({4, 5});
      inputs.push_back({4, 6});
      inputs.push_back({5, 6});
      inputs.push_back({7, 8});
      inputs.push_back({12, 10});
      inputs.push_back({12, 13});
      inputs.push_back({9, 10});
      inputs.push_back({10, 11});
      inputs.push_back({10, 12});
      inputs.push_back({11, 12});
      inputs.push_back({13, 14});
      inputs.push_back({14, 15});
      inputs.push_back({14, 16});
      inputs.push_back({16, 17});
      inputs.push_back({16, 18});
      inputs.push_back({18, 19});
      inputs.push_back({17, 19});
      inputs.push_back({15, 19});
      inputs.push_back({19, 20});
      inputs.push_back({19, 21});
      inputs.push_back({21, 22});
      inputs.push_back({21, 23});
      inputs.push_back({23, 24});
      inputs.push_back({22, 24});
      inputs.push_back({20, 24});
      inputs.push_back({24, 25});
      inputs.push_back({24, 26});
      inputs.push_back({26, 27});
      inputs.push_back({26, 28});
      inputs.push_back({28, 29});
      inputs.push_back({27, 29});
      inputs.push_back({25, 29});
      inputs.push_back({29, 30});
      inputs.push_back({29, 31});
      inputs.push_back({31, 32});
      inputs.push_back({31, 33});
      inputs.push_back({32, 33});
      inputs.push_back({30, 33});
      inputs.push_back({33, 34});
      inputs.push_back({33, 35});
      inputs.push_back({35, 37});
      inputs.push_back({35, 36});
      inputs.push_back({36, 37});
      inputs.push_back({34, 37});
      inputs.push_back({37, 38});
      inputs.push_back({37, 39});
      inputs.push_back({39, 40});
      inputs.push_back({39, 41});
      inputs.push_back({40, 41});
      inputs.push_back({38, 41});
      inputs.push_back({41, 42});
      inputs.push_back({41, 43});
      inputs.push_back({42, 43});
      inputs.push_back({43, 44});
      inputs.push_back({43, 45});
      inputs.push_back({44, 45});
      inputs.push_back({45, 46});
      inputs.push_back({45, 47});
      inputs.push_back({46, 47});
      inputs.push_back({47, 48});
      inputs.push_back({47, 49});
      inputs.push_back({48, 49});
      inputs.push_back({49, 50});
      inputs.push_back({49, 51});
      inputs.push_back({50, 51});
      inputs.push_back({51, 52});
      inputs.push_back({51, 53});
      inputs.push_back({52, 53});
      inputs.push_back({53, 54});
      inputs.push_back({53, 55});
      inputs.push_back({54, 55});
      inputs.push_back({55, 56});
      inputs.push_back({55, 57});
      inputs.push_back({56, 57});
      inputs.push_back({57, 58});
      inputs.push_back({57, 59});
      inputs.push_back({58, 59});
      inputs.push_back({59, 60});
      inputs.push_back({59, 61});
      inputs.push_back({60, 61});
      inputs.push_back({61, 62});
      inputs.push_back({61, 63});
      inputs.push_back({62, 63});
      inputs.push_back({63, 64});
      inputs.push_back({63, 65});
      inputs.push_back({65, 66});
      inputs.push_back({65, 69});
      inputs.push_back({68, 67});
      inputs.push_back({68, 68});
      inputs.push_back({66, 68});
      inputs.push_back({67, 69});
      inputs.push_back({69, 70});
      inputs.push_back({69, 71});
      inputs.push_back({70, 71});
      inputs.push_back({71, 72});
      inputs.push_back({71, 73});
      inputs.push_back({72, 73});
      inputs.push_back({73, 74});
      inputs.push_back({73, 75});
      inputs.push_back({74, 75});
      inputs.push_back({75, 76});
      inputs.push_back({75, 77});
      inputs.push_back({76, 77});
      inputs.push_back({77, 78});
      inputs.push_back({77, 79});
      inputs.push_back({78, 79});
      controlFlow.Construct(inputs);
      uniformBlocks = controlFlow.GetUniformBlocks();
      REQUIRE(28 == uniformBlocks.count());
      REQUIRE(uniformBlocks.contains(0U));
      REQUIRE(uniformBlocks.contains(2U));
      REQUIRE(uniformBlocks.contains(8U));
      REQUIRE(uniformBlocks.contains(14U));
      REQUIRE(uniformBlocks.contains(19U));
      REQUIRE(uniformBlocks.contains(24U));
      REQUIRE(uniformBlocks.contains(29U));
      REQUIRE(uniformBlocks.contains(33U));
      REQUIRE(uniformBlocks.contains(37U));
      REQUIRE(uniformBlocks.contains(41U));
      REQUIRE(uniformBlocks.contains(43U));
      REQUIRE(uniformBlocks.contains(45U));
      REQUIRE(uniformBlocks.contains(47U));
      REQUIRE(uniformBlocks.contains(49U));
      REQUIRE(uniformBlocks.contains(51U));
      REQUIRE(uniformBlocks.contains(53U));
      REQUIRE(uniformBlocks.contains(55U));
      REQUIRE(uniformBlocks.contains(57U));
      REQUIRE(uniformBlocks.contains(59U));
      REQUIRE(uniformBlocks.contains(61U));
      REQUIRE(uniformBlocks.contains(63U));
      REQUIRE(uniformBlocks.contains(65U));
      REQUIRE(uniformBlocks.contains(69U));
      REQUIRE(uniformBlocks.contains(71U));
      REQUIRE(uniformBlocks.contains(73U));
      REQUIRE(uniformBlocks.contains(75U));
      REQUIRE(uniformBlocks.contains(77U));
      REQUIRE(uniformBlocks.contains(79U));
      loopBlocks = controlFlow.GetLoopBlocks();
      REQUIRE(7 == loopBlocks.count());
      REQUIRE(loopBlocks.contains(4U));
      REQUIRE(loopBlocks.contains(5U));
      REQUIRE(loopBlocks.contains(6U));
      REQUIRE(loopBlocks.contains(10U));
      REQUIRE(loopBlocks.contains(11U));
      REQUIRE(loopBlocks.contains(12U));
      REQUIRE(loopBlocks.contains(68U));
      CheckUniformBlocks(controlFlow);
    }
    SECTION("Single Loop Specific Setup")
    {
      // Specific loop case where a block (2) in a loop is only in a single path
      // 0 -> 1 -> 3 - 1
      // 0 -> 1 -> 2 -> 3
      // 3 -> 4 -> END
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 3});
      inputs.push_back({3, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      controlFlow.Construct(inputs);
      uniformBlocks = controlFlow.GetUniformBlocks();
      REQUIRE(2 == uniformBlocks.count());
      REQUIRE(uniformBlocks.contains(0U));
      REQUIRE(uniformBlocks.contains(4U));
      loopBlocks = controlFlow.GetLoopBlocks();
      REQUIRE(3 == loopBlocks.count());
      REQUIRE(loopBlocks.contains(1U));
      REQUIRE(loopBlocks.contains(2U));
      REQUIRE(loopBlocks.contains(3U));
      CheckUniformBlocks(controlFlow);
    }
  };
  SECTION("FindConvergenceBlocks")
  {
    ControlFlow controlFlow;
    BlockArray divergentBlocks;
    rdcarray<ConvergentBlockData> convergentBlocks;
    rdcarray<ConvergentBlockData> expectedConvergentBlocks;
    int32_t expectedCountDivergentBlocks = 0;
    SECTION("Degenerate Case")
    {
      // Degenerate case
      rdcarray<BlockLink> inputs;

      expectedCountDivergentBlocks = 0;
      expectedConvergentBlocks = {};
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }

    SECTION("Just Start and End")
    {
      // No divergent blocks
      // 0 -> 1
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});

      expectedCountDivergentBlocks = 0;
      expectedConvergentBlocks = {};
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("Single Branch")
    {
      // One divergent block : 0
      // One convergent block : 0->3
      // 0 -> 1 -> 3
      // 0 -> 2 -> 3
      // 3 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 3});
      inputs.push_back({0, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});

      expectedCountDivergentBlocks = 1;
      expectedConvergentBlocks = {
          {0, 3},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("Simple Double Branch")
    {
      // Two divergent blocks : 0, 2
      // Two convergent blocks : 0->2, 2->4
      // 0 -> 1
      // 0 -> 2
      // 1 -> 2
      // 2 -> 3 -> 4
      // 2 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 4});

      expectedCountDivergentBlocks = 2;
      expectedConvergentBlocks = {
          {0, 2},
          {2, 4},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("Nested Branch")
    {
      // Two divergent blocks : 0, 3
      // Two convergent blocks : 0->9, 3->8
      // 0 -> 1
      // 0 -> 2
      // 1 -> 3
      // 3 -> 4
      // 3 -> 5
      // 4 -> 6
      // 5 -> 7
      // 6 -> 8
      // 7 -> 8
      // 8 -> 9
      // 2 -> 9
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 3});
      inputs.push_back({3, 4});
      inputs.push_back({3, 5});
      inputs.push_back({4, 6});
      inputs.push_back({5, 7});
      inputs.push_back({6, 8});
      inputs.push_back({7, 8});
      inputs.push_back({8, 9});
      inputs.push_back({2, 9});

      expectedCountDivergentBlocks = 2;
      expectedConvergentBlocks = {
          {0, 9},
          {3, 8},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("Nested Linked Branch")
    {
      // Three divergent blocks : 0, 3, 4
      // Three convergent blocks : 0->13, 3->11, 4->13
      // 0 -> 1
      // 0 -> 2
      // 1 -> 3
      // 2 -> 4
      // 3 -> 5
      // 3 -> 6
      // 4 -> 6
      // 4 -> 7
      // 5 -> 8
      // 6 -> 9
      // 7 -> 10
      // 8 -> 11
      // 9 -> 11
      // 11 -> 12
      // 12 -> 13
      // 10 -> 13
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 5});
      inputs.push_back({3, 6});
      inputs.push_back({4, 6});
      inputs.push_back({4, 7});
      inputs.push_back({5, 8});
      inputs.push_back({6, 9});
      inputs.push_back({7, 10});
      inputs.push_back({8, 11});
      inputs.push_back({9, 11});
      inputs.push_back({11, 12});
      inputs.push_back({12, 13});
      inputs.push_back({10, 13});

      expectedCountDivergentBlocks = 3;
      expectedConvergentBlocks = {
          {0, 13},
          {3, 11},
          {4, 13},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("Simple Loop")
    {
      // One divergent block : 2
      // One convergent block : 2->3
      // 0 -> 1
      // 1 -> 2
      // 2 -> 1
      // 2 -> 3
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 1});
      inputs.push_back({2, 3});

      expectedCountDivergentBlocks = 1;
      expectedConvergentBlocks = {
          {2, 3},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("Loop with multiple exits")
    {
      // Two divergent blocks : 2, 3
      // Two convergent blocks : 2->6, 3->6
      // 0 -> 1
      // 1 -> 2
      // 2 -> 3
      // 2 -> 4
      // 3 -> 1
      // 3 -> 6
      // 4 -> 5
      // 5 -> 6
      // 6 -> 7

      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 1});
      inputs.push_back({3, 6});
      inputs.push_back({4, 5});
      inputs.push_back({5, 6});
      inputs.push_back({6, 7});

      expectedCountDivergentBlocks = 2;
      expectedConvergentBlocks = {
          {2, 6},
          {3, 6},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("Multiple Loops with multiple exits")
    {
      // Three divergent blocks : 2, 3, 5
      // Three convergent blocks : 2->6, 3->6, 5->6
      // 0 -> 1
      // 1 -> 2
      // 2 -> 3
      // 2 -> 4
      // 3 -> 1
      // 3 -> 6
      // 4 -> 5
      // 5 -> 6
      // 5 -> 7
      // 7 -> 2
      // 6 -> 8

      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 1});
      inputs.push_back({3, 6});
      inputs.push_back({4, 5});
      inputs.push_back({5, 6});
      inputs.push_back({5, 7});
      inputs.push_back({7, 2});
      inputs.push_back({6, 8});

      expectedCountDivergentBlocks = 3;
      expectedConvergentBlocks = {
          {2, 6},
          {3, 6},
          {5, 6},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("If inside a Loop")
    {
      // Two divergent blocks : 2, 6
      // Two convergent blocks : 2->5, 6->7
      // 0 -> 1
      // 1 -> 2
      // 2 -> 3
      // 2 -> 4
      // 3 -> 5
      // 4 -> 5
      // 5 -> 6
      // 6 -> 1
      // 6 -> 7

      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 5});
      inputs.push_back({4, 5});
      inputs.push_back({5, 6});
      inputs.push_back({6, 1});
      inputs.push_back({6, 7});

      expectedCountDivergentBlocks = 2;
      expectedConvergentBlocks = {
          {2, 5},
          {6, 7},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("Single Uniform Flow")
    {
      // Single uniform flow between start and end
      // 0 -> 1 -> 2 -> 3 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      controlFlow.Construct(inputs);

      expectedCountDivergentBlocks = 0;
      expectedConvergentBlocks = {};
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("Infinite Loop")
    {
      // Infinite loop which never converges (3 -> 4 -> 3)
      // 0 -> 1 -> 3
      // 0 -> 2 -> 3
      // 3 -> 4
      // 4 -> 3
      // 1 -> 6
      // 2 -> 6
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 3});
      inputs.push_back({0, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      inputs.push_back({4, 3});
      inputs.push_back({1, 6});
      inputs.push_back({2, 6});
      controlFlow.Construct(inputs);

      expectedCountDivergentBlocks = 3;
      expectedConvergentBlocks = {
          {0, 6},
          {1, 6},
          {2, 6},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }

    SECTION("Complex Case Two Loops")
    {
      // Complex case with two loops
      // Loop: 7 -> 9 -> 7, 13 -> 15 -> 13
      // 0 -> 1 -> 3
      // 0 -> 2 -> 3
      // 3 -> 4 -> 5
      // 3 -> 5
      // 5 -> 6 -> 7
      // 5 -> 11
      // 7 -> 8 -> 11
      // 7 -> 9 -> 7
      // 9 -> 10 -> 11 -> 12 -> 13 -> 14 -> 17 -> 18 -> 19 -> 20 -> 21 -> 22 -> 23 -> 26
      // 13 -> 15 -> 13
      // 15 -> 16 -> 17 -> 19 -> 21 -> 26
      // 11 -> 17
      // 22 -> 24 -> 25 -> 26
      // 24 -> 26
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({2, 3});
      inputs.push_back({1, 3});
      inputs.push_back({3, 4});
      inputs.push_back({4, 5});
      inputs.push_back({3, 5});
      inputs.push_back({5, 6});
      inputs.push_back({9, 7});
      inputs.push_back({6, 7});
      inputs.push_back({7, 8});
      inputs.push_back({7, 9});
      inputs.push_back({9, 10});
      inputs.push_back({10, 11});
      inputs.push_back({8, 11});
      inputs.push_back({5, 11});
      inputs.push_back({11, 12});
      inputs.push_back({15, 13});
      inputs.push_back({12, 13});
      inputs.push_back({13, 14});
      inputs.push_back({13, 15});
      inputs.push_back({15, 16});
      inputs.push_back({16, 17});
      inputs.push_back({14, 17});
      inputs.push_back({11, 17});
      inputs.push_back({17, 18});
      inputs.push_back({18, 19});
      inputs.push_back({17, 19});
      inputs.push_back({19, 20});
      inputs.push_back({20, 21});
      inputs.push_back({19, 21});
      inputs.push_back({21, 22});
      inputs.push_back({22, 23});
      inputs.push_back({22, 24});
      inputs.push_back({24, 25});
      inputs.push_back({25, 26});
      inputs.push_back({24, 26});
      inputs.push_back({23, 26});
      inputs.push_back({21, 26});
      controlFlow.Construct(inputs);

      expectedCountDivergentBlocks = 13;
      expectedConvergentBlocks = {
          {0, 3},   {3, 5},   {5, 11},  {7, 11},  {9, 11},  {11, 17}, {13, 17},
          {15, 17}, {17, 19}, {19, 21}, {21, 26}, {22, 26}, {24, 26},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
    SECTION("Complex Case Multiple Loops")
    {
      // Complex case with multiple loops: 4 -> 5 -> 6 -> 4, 10 -> 11 -> 12 -> 10, 68
      // 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 4
      // 0 -> 2 -> 8 -> 9 -> 10 -> 11 -> 12 -> 10
      // 4 -> 6 -> 7 -> 8 -> 14 -> 15 -> 19 -> 20 -> 24 -> 25 -> 29 -> 31 -> 32 -> 33
      // 10 -> 12 -> 13 -> 14 -> 16 -> 17 -> 19
      // 16 -> 18 -> 19 -> 21 -> 22 -> 23 -> 24 -> 26 -> 27 -> 29 -> 30 -> 33 -> 35 -> 37
      // 22 -> 24
      // 26 -> 28 -> 29

      // 31 -> 33 -> 34 -> 37 -> 39 -> 40 -> 41 -> 42 -> 43 -> 44 -> 45 -> 47 -> 49 -> 51 -> 52 ->
      //   53 -> 54 -> 55 -> 57 -> 58 -> 59 -> 60 -> 61 -> 62 -> 63 -> 64 -> 65 -> 66 -> 68 -> 67
      //   -> 69 -> 70 -> 71 -> 72 -> 73 -> 74 -> 75 -> 76 -> 77 -> 78 -> 79 -> END

      // 35 -> 36 -> 37 -> 38 -> 41 39 -> 41 -> 43 -> 45 -> 46 -> 47 -> 48 -> 49 -> 50 -> 51
      // 51 -> 53 -> 55 -> 56 -> 57 -> 59 -> 61 -> 63 -> 65 -> 69 -> 71 -> 73 -> 75 -> 77 -> 79
      // 68 -> 68
      rdcarray<BlockLink> inputs;
      inputs.push_back({8, 9});
      inputs.push_back({8, 14});
      inputs.push_back({64, 65});
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 8});
      inputs.push_back({6, 4});
      inputs.push_back({6, 7});
      inputs.push_back({3, 4});
      inputs.push_back({4, 5});
      inputs.push_back({4, 6});
      inputs.push_back({5, 6});
      inputs.push_back({7, 8});
      inputs.push_back({12, 10});
      inputs.push_back({12, 13});
      inputs.push_back({9, 10});
      inputs.push_back({10, 11});
      inputs.push_back({10, 12});
      inputs.push_back({11, 12});
      inputs.push_back({13, 14});
      inputs.push_back({14, 15});
      inputs.push_back({14, 16});
      inputs.push_back({16, 17});
      inputs.push_back({16, 18});
      inputs.push_back({18, 19});
      inputs.push_back({17, 19});
      inputs.push_back({15, 19});
      inputs.push_back({19, 20});
      inputs.push_back({19, 21});
      inputs.push_back({21, 22});
      inputs.push_back({21, 23});
      inputs.push_back({23, 24});
      inputs.push_back({22, 24});
      inputs.push_back({20, 24});
      inputs.push_back({24, 25});
      inputs.push_back({24, 26});
      inputs.push_back({26, 27});
      inputs.push_back({26, 28});
      inputs.push_back({28, 29});
      inputs.push_back({27, 29});
      inputs.push_back({25, 29});
      inputs.push_back({29, 30});
      inputs.push_back({29, 31});
      inputs.push_back({31, 32});
      inputs.push_back({31, 33});
      inputs.push_back({32, 33});
      inputs.push_back({30, 33});
      inputs.push_back({33, 34});
      inputs.push_back({33, 35});
      inputs.push_back({35, 37});
      inputs.push_back({35, 36});
      inputs.push_back({36, 37});
      inputs.push_back({34, 37});
      inputs.push_back({37, 38});
      inputs.push_back({37, 39});
      inputs.push_back({39, 40});
      inputs.push_back({39, 41});
      inputs.push_back({40, 41});
      inputs.push_back({38, 41});
      inputs.push_back({41, 42});
      inputs.push_back({41, 43});
      inputs.push_back({42, 43});
      inputs.push_back({43, 44});
      inputs.push_back({43, 45});
      inputs.push_back({44, 45});
      inputs.push_back({45, 46});
      inputs.push_back({45, 47});
      inputs.push_back({46, 47});
      inputs.push_back({47, 48});
      inputs.push_back({47, 49});
      inputs.push_back({48, 49});
      inputs.push_back({49, 50});
      inputs.push_back({49, 51});
      inputs.push_back({50, 51});
      inputs.push_back({51, 52});
      inputs.push_back({51, 53});
      inputs.push_back({52, 53});
      inputs.push_back({53, 54});
      inputs.push_back({53, 55});
      inputs.push_back({54, 55});
      inputs.push_back({55, 56});
      inputs.push_back({55, 57});
      inputs.push_back({56, 57});
      inputs.push_back({57, 58});
      inputs.push_back({57, 59});
      inputs.push_back({58, 59});
      inputs.push_back({59, 60});
      inputs.push_back({59, 61});
      inputs.push_back({60, 61});
      inputs.push_back({61, 62});
      inputs.push_back({61, 63});
      inputs.push_back({62, 63});
      inputs.push_back({63, 64});
      inputs.push_back({63, 65});
      inputs.push_back({65, 66});
      inputs.push_back({65, 69});
      inputs.push_back({68, 67});
      inputs.push_back({68, 68});
      inputs.push_back({66, 68});
      inputs.push_back({67, 69});
      inputs.push_back({69, 70});
      inputs.push_back({69, 71});
      inputs.push_back({70, 71});
      inputs.push_back({71, 72});
      inputs.push_back({71, 73});
      inputs.push_back({72, 73});
      inputs.push_back({73, 74});
      inputs.push_back({73, 75});
      inputs.push_back({74, 75});
      inputs.push_back({75, 76});
      inputs.push_back({75, 77});
      inputs.push_back({76, 77});
      inputs.push_back({77, 78});
      inputs.push_back({77, 79});
      inputs.push_back({78, 79});
      controlFlow.Construct(inputs);

      expectedCountDivergentBlocks = 38;
      expectedConvergentBlocks = {
          {0, 2},   {2, 8},   {4, 6},   {6, 7},   {8, 14},  {10, 12}, {12, 13}, {14, 19},
          {16, 19}, {19, 24}, {21, 24}, {24, 29}, {26, 29}, {29, 33}, {31, 33}, {33, 37},
          {35, 37}, {37, 41}, {39, 41}, {41, 43}, {43, 45}, {45, 47}, {47, 49}, {49, 51},
          {51, 53}, {53, 55}, {55, 57}, {57, 59}, {59, 61}, {61, 63}, {63, 65}, {65, 69},
          {68, 67}, {69, 71}, {71, 73}, {73, 75}, {75, 77}, {77, 79},
      };
      REQUIRE(expectedConvergentBlocks.count() == expectedCountDivergentBlocks);

      controlFlow.Construct(inputs);
      divergentBlocks = controlFlow.GetDivergentBlocks();
      convergentBlocks = controlFlow.GetConvergentBlocks();
      REQUIRE(expectedCountDivergentBlocks == divergentBlocks.count());
      REQUIRE(expectedCountDivergentBlocks == convergentBlocks.count());
      CheckDivergentBlocks(expectedConvergentBlocks, divergentBlocks);
      CheckConvergentBlocks(expectedConvergentBlocks, convergentBlocks);
    }
  };
  SECTION("FindPartialConvergenceBlocks")
  {
    ControlFlow controlFlow;
    rdcarray<PartialConvergentBlockData> partialConvergentBlocks;
    rdcarray<PartialConvergentBlockData> expectedPartialConvergentBlocks;

    SECTION("Degenerate Case")
    {
      // Degenerate case
      rdcarray<BlockLink> inputs;

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }

    SECTION("Just Start and End")
    {
      // No divergent blocks
      // 0 -> 1
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Single Branch")
    {
      // No partial convergence blocks
      // 0 -> 1 -> 3
      // 0 -> 2 -> 3
      // 3 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 3});
      inputs.push_back({0, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Simple Double Branch")
    {
      // No partial convergence blocks
      // 0 -> 1
      // 0 -> 2
      // 1 -> 2
      // 2 -> 3 -> 4
      // 2 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 4});

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Nested Branch")
    {
      // No partial convergence blocks
      // 0 -> 1
      // 0 -> 2
      // 1 -> 3
      // 3 -> 4
      // 3 -> 5
      // 4 -> 6
      // 5 -> 7
      // 6 -> 8
      // 7 -> 8
      // 8 -> 9
      // 2 -> 9
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 3});
      inputs.push_back({3, 4});
      inputs.push_back({3, 5});
      inputs.push_back({4, 6});
      inputs.push_back({5, 7});
      inputs.push_back({6, 8});
      inputs.push_back({7, 8});
      inputs.push_back({8, 9});
      inputs.push_back({2, 9});

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Nested Linked Branch")
    {
      // One partial convergence block: 0 -> 6
      // 0 -> 1
      // 0 -> 2
      // 1 -> 3
      // 2 -> 4
      // 3 -> 5
      // 3 -> 6
      // 4 -> 6
      // 4 -> 7
      // 5 -> 8
      // 6 -> 9
      // 7 -> 10
      // 8 -> 11
      // 9 -> 11
      // 11 -> 12
      // 12 -> 13
      // 10 -> 13
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 5});
      inputs.push_back({3, 6});
      inputs.push_back({4, 6});
      inputs.push_back({4, 7});
      inputs.push_back({5, 8});
      inputs.push_back({6, 9});
      inputs.push_back({7, 10});
      inputs.push_back({8, 11});
      inputs.push_back({9, 11});
      inputs.push_back({11, 12});
      inputs.push_back({12, 13});
      inputs.push_back({10, 13});

      expectedPartialConvergentBlocks = {
          {0, {6}},
      };

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Simple Loop")
    {
      // No partial convergence blocks
      // 0 -> 1
      // 1 -> 2
      // 2 -> 1
      // 2 -> 3
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 1});
      inputs.push_back({2, 3});

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Loop with multiple exits")
    {
      // No partial convergence blocks
      // 0 -> 1
      // 1 -> 2
      // 2 -> 3
      // 2 -> 4
      // 3 -> 1
      // 3 -> 6
      // 4 -> 5
      // 5 -> 6
      // 6 -> 7

      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 1});
      inputs.push_back({3, 6});
      inputs.push_back({4, 5});
      inputs.push_back({5, 6});
      inputs.push_back({6, 7});

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Multiple Loops with multiple exits")
    {
      // No partial convergence blocks
      // 0 -> 1
      // 1 -> 2
      // 2 -> 3
      // 2 -> 4
      // 3 -> 1
      // 3 -> 6
      // 4 -> 5
      // 5 -> 6
      // 5 -> 7
      // 7 -> 2
      // 6 -> 8

      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 1});
      inputs.push_back({3, 6});
      inputs.push_back({4, 5});
      inputs.push_back({5, 6});
      inputs.push_back({5, 7});
      inputs.push_back({7, 2});
      inputs.push_back({6, 8});

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("If inside a Loop")
    {
      // No partial convergence blocks
      // 0 -> 1
      // 1 -> 2
      // 2 -> 3
      // 2 -> 4
      // 3 -> 5
      // 4 -> 5
      // 5 -> 6
      // 6 -> 1
      // 6 -> 7

      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 5});
      inputs.push_back({4, 5});
      inputs.push_back({5, 6});
      inputs.push_back({6, 1});
      inputs.push_back({6, 7});

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Single Uniform Flow")
    {
      // No partial convergence blocks
      // Single uniform flow between start and end
      // 0 -> 1 -> 2 -> 3 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      controlFlow.Construct(inputs);

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Infinite Loop")
    {
      // Infinite loop which never converges (3 -> 4 -> 3)
      // No partial convergence blocks
      // 0 -> 1 -> 3
      // 0 -> 2 -> 3
      // 3 -> 4
      // 4 -> 3
      // 1 -> 6
      // 2 -> 6
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 3});
      inputs.push_back({0, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      inputs.push_back({4, 3});
      inputs.push_back({1, 6});
      inputs.push_back({2, 6});
      controlFlow.Construct(inputs);

      expectedPartialConvergentBlocks = {};

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Nested Branch")
    {
      // One partial convergence block: 0 -> 2
      // 0 -> 1
      // 0 -> 2
      // 1 -> 2
      // 1 -> 3
      // 2 -> 4
      // 3 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 2});
      inputs.push_back({1, 3});
      inputs.push_back({2, 4});
      inputs.push_back({3, 4});
      controlFlow.Construct(inputs);

      expectedPartialConvergentBlocks = {
          {0, {2}},
      };

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Nested Branch After Root Divergence")
    {
      // One partial convergence block: 1 -> 5
      // 0 -> 1
      // 0 -> 2
      // 1 -> 3
      // 1 -> 4
      // 2 -> 6
      // 3 -> 5
      // 4 -> 5
      // 4 -> 6
      // 5 -> 6
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 3});
      inputs.push_back({1, 4});
      inputs.push_back({2, 6});
      inputs.push_back({3, 5});
      inputs.push_back({4, 5});
      inputs.push_back({4, 6});
      inputs.push_back({5, 6});
      controlFlow.Construct(inputs);

      expectedPartialConvergentBlocks = {
          {1, {5}},
      };

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Multiple Partial Convergence Points To Different Blocks")
    {
      // Two partial convergence blocks: 1 -> 7, 6 (ordered least to most connections)
      // Single partial convergence blocks: 3 -> 7
      // 0 -> 1
      // 0 -> 2
      // 1 -> 3
      // 1 -> 4
      // 2 -> 8
      // 3 -> 5
      // 3 -> 6
      // 4 -> 6
      // 4 -> 8
      // 5 -> 7
      // 6 -> 7
      // 6 -> 8
      // 7 -> 8

      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 3});
      inputs.push_back({1, 4});
      inputs.push_back({2, 8});
      inputs.push_back({3, 5});
      inputs.push_back({3, 6});
      inputs.push_back({4, 6});
      inputs.push_back({4, 8});
      inputs.push_back({5, 7});
      inputs.push_back({6, 7});
      inputs.push_back({6, 8});
      inputs.push_back({7, 8});
      controlFlow.Construct(inputs);

      expectedPartialConvergentBlocks = {
          {1, {7, 6}},
          {3, {7}},
      };

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
    SECTION("Multiple Partial Convergence Points To The Same Block")
    {
      // Single partial convergence blocks: 0 -> 4, 1 -> 4, 2 -> 4
      // 0 -> 1
      // 0 -> 2
      // 1 -> 3
      // 1 -> 4
      // 2 -> 4
      // 2 -> 5
      // 3 -> 4
      // 3 -> 6
      // 4 -> 6
      // 5 -> 6
      // 5 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({0, 2});
      inputs.push_back({1, 3});
      inputs.push_back({1, 4});
      inputs.push_back({2, 4});
      inputs.push_back({2, 5});
      inputs.push_back({3, 4});
      inputs.push_back({3, 6});
      inputs.push_back({4, 6});
      inputs.push_back({5, 6});
      inputs.push_back({5, 4});
      controlFlow.Construct(inputs);

      expectedPartialConvergentBlocks = {
          {0, {4}},
          {1, {4}},
          {2, {4}},
      };

      controlFlow.Construct(inputs);
      partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      CheckPartialConvergentBlocks(expectedPartialConvergentBlocks, partialConvergentBlocks);
    }
  };
};
#endif    // ENABLED(ENABLE_UNIT_TESTS)
