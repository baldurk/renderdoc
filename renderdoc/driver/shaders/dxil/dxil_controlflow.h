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

#pragma once

#include <unordered_set>

namespace DXIL
{
typedef rdcarray<uint32_t> BlockArray;
typedef rdcpair<uint32_t, uint32_t> BlockLink;
typedef rdcpair<uint32_t, uint32_t> ConvergentBlockData;
typedef rdcpair<uint32_t, BlockArray> PartialConvergentBlockData;

struct ControlFlow
{
public:
  ControlFlow() = default;
  ControlFlow(const rdcarray<rdcpair<uint32_t, uint32_t>> &links) { Construct(links); }
  void Construct(const rdcarray<BlockLink> &links);
  const std::unordered_set<uint32_t> &GetBlocks() const { return m_Blocks; }
  rdcarray<uint32_t> GetUniformBlocks() const { return m_UniformBlocks; }
  rdcarray<uint32_t> GetLoopBlocks() const { return m_LoopBlocks; }
  rdcarray<uint32_t> GetDivergentBlocks() const { return m_DivergentBlocks; }
  rdcarray<ConvergentBlockData> GetConvergentBlocks() const { return m_ConvergentBlocks; }
  rdcarray<PartialConvergentBlockData> GetPartialConvergentBlocks() const
  {
    return m_PartialConvergentBlocks;
  }
  uint32_t GetNextUniformBlock(uint32_t from) const;
  bool IsConnected(uint32_t from, uint32_t to) const;

private:
  typedef rdcarray<uint32_t> BlockPath;

  enum class ConnectionState : uint8_t
  {
    Unknown,
    NotConnected,
    Connected,
  };

  struct Node
  {
    uint32_t blockId;
    uint32_t idx;
    rdcarray<Node *> parents;
    rdcarray<Node *> children;
  };

  const Node *GetNode(const uint32_t blockId) const
  {
    return m_Nodes.data() + m_BlockIDsToIDx[blockId];
  }

  bool BlockInAllPaths(uint32_t from, uint32_t to) const;
  bool BlockInMultiplePaths(uint32_t from, uint32_t to) const;
  int32_t BlockInAnyPath(uint32_t from, uint32_t to) const;

  bool AnyPath(uint32_t from, uint32_t to) const;
  bool AllPathsContainBlock(uint32_t from, uint32_t to, uint32_t mustInclude) const;

  uint32_t PATH_END = ~0U;

  rdcarray<Node> m_Nodes;
  rdcarray<uint32_t> m_BlockIDsToIDx;
  mutable rdcarray<uint8_t> m_TracedNodes;

  std::unordered_set<uint32_t> m_Blocks;

  rdcarray<uint32_t> m_UniformBlocks;
  rdcarray<uint32_t> m_LoopBlocks;
  rdcarray<uint32_t> m_DivergentBlocks;
  rdcarray<ConvergentBlockData> m_ConvergentBlocks;
  rdcarray<PartialConvergentBlockData> m_PartialConvergentBlocks;
  mutable rdcarray<rdcarray<ConnectionState>> m_Connections;

  friend rdcstr GenerateGraph(const char *const name, const ControlFlow *graph);
};
};    // namespace DXIL
