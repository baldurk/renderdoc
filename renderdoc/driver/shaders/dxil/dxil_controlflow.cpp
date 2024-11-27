/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

#include <unordered_map>
#include <unordered_set>

#include "dxil_controlflow.h"

/*

Inputs are links of blocks : from -> to (can be forwards or backwards links)
Output is a list of uniform control flow blocks which all possible flows go through (not diverged)
and are not in a loop

The algorithm is:

1. Setup
  * Compute all possible known blocks.
  * For each block generate a list of "to" blocks from the input links
  * Any block without links in the input are set to link to the end sentinel (PATH_END)

2. Generate all possible paths
  * Paths can terminate at the end block (PATH_END)
  * Paths can also terminate at a block before the end, if that block has had all its possible paths
already computed

3. Find Uniform Blocks
  * Generate a list of path indexes for each block in the paths
  * Generate a list of all paths blocks which are blocks which appear in all possible paths
    * all paths includes walking any paths linked at the end node of the path being walked
  * Generate a list of loop blocks which are blocks which appear in any path starting from the block
  * uniform blocks are defined to be blocks which are all paths blocks minus loop blocks
*/

namespace DXIL
{
struct ControlFlow
{
public:
  ControlFlow(const rdcarray<rdcpair<uint32_t, uint32_t>> &links);
  void FindUniformBlocks(rdcarray<uint32_t> &uniformBlocks);

private:
  typedef rdcarray<uint32_t> BlockPath;

  bool TraceBlockFlow(const uint32_t from, BlockPath &path);
  bool BlockInAllPaths(uint32_t block, uint32_t pathIdx, uint32_t startIdx);
  bool BlockInAnyPath(uint32_t block, uint32_t pathIdx, uint32_t startIdx);

  const uint32_t PATH_END = ~0U;

  std::unordered_set<uint32_t> m_Blocks;
  std::unordered_map<uint32_t, BlockPath> m_BlockLinks;

  std::unordered_map<uint32_t, rdcarray<uint32_t>> m_BlockPathLinks;
  std::unordered_set<uint32_t> m_TracedBlocks;
  std::unordered_set<uint32_t> m_CheckedPaths;
  rdcarray<BlockPath> m_Paths;
};

ControlFlow::ControlFlow(const rdcarray<rdcpair<uint32_t, uint32_t>> &links)
{
  // 1. Setup
  // Compute all possible known blocks
  for(const auto &link : links)
  {
    uint32_t from = link.first;
    uint32_t to = link.second;
    m_Blocks.insert(from);
    m_Blocks.insert(to);
  }

  // For each block a list of "to" blocks
  for(const auto &link : links)
  {
    uint32_t from = link.first;
    uint32_t to = link.second;
    m_BlockLinks[from].push_back(to);
  }

  // Any block without links in the input are set to link to the end sentinel (PATH_END)
  for(uint32_t b : m_Blocks)
  {
    if(m_BlockLinks.count(b) == 0)
      m_BlockLinks[b].push_back(PATH_END);
  }

  // 2. Generate all possible paths

  // Paths can terminate at the end block (PATH_END)
  // Paths can also terminate at a block before the end, if that block has had all its possible paths already computed
  for(const auto &it : m_BlockLinks)
  {
    uint32_t from = it.first;
    if(m_TracedBlocks.count(from) != 0)
      continue;
    BlockPath path;
    path.push_back(from);
    TraceBlockFlow(from, path);
  }
}

bool ControlFlow::TraceBlockFlow(const uint32_t from, BlockPath &path)
{
  if(m_BlockLinks.count(from) == 0)
  {
    m_Paths.push_back(path);
    return true;
  }
  if(m_TracedBlocks.count(from) != 0)
  {
    m_Paths.push_back(path);
    return true;
  }
  m_TracedBlocks.insert(from);
  BlockPath newPath = path;
  const rdcarray<uint32_t> &gotos = m_BlockLinks.at(from);
  for(uint32_t to : gotos)
  {
    newPath.push_back(to);
    if(TraceBlockFlow(to, newPath))
      newPath = path;
  }
  return true;
}

bool ControlFlow::BlockInAnyPath(uint32_t block, uint32_t pathIdx, uint32_t startIdx)
{
  const BlockPath &path = m_Paths[pathIdx];
  if(path.size() == 0)
    return false;

  // Check the current path
  for(uint32_t i = startIdx; i < path.size(); ++i)
  {
    if(block == path[i])
      return true;
  }

  uint32_t endNode = path[path.size() - 1];
  if(endNode == PATH_END)
    return false;

  m_CheckedPaths.insert(endNode);

  // Check any paths linked to by the end node of the current path
  const rdcarray<uint32_t> &childPathsToCheck = m_BlockPathLinks[endNode];
  for(uint32_t childPathIdx : childPathsToCheck)
  {
    if(m_CheckedPaths.count(childPathIdx) != 0)
      continue;

    m_CheckedPaths.insert(childPathIdx);
    const BlockPath &childPath = m_Paths[childPathIdx];
    uint32_t childPartStartIdx = ~0U;
    for(uint32_t i = 0; i < childPath.size(); ++i)
    {
      if(childPath[i] == endNode)
      {
        childPartStartIdx = i;
        break;
      }
    }
    if(childPartStartIdx != ~0U)
    {
      if(BlockInAnyPath(block, childPathIdx, childPartStartIdx))
        return true;
    }
  }
  return false;
}

bool ControlFlow::BlockInAllPaths(uint32_t block, uint32_t pathIdx, uint32_t startIdx)
{
  const BlockPath &path = m_Paths[pathIdx];
  if(path.size() == 0)
    return false;

  // Check the current path
  for(uint32_t i = startIdx; i < path.size(); ++i)
  {
    if(block == path[i])
      return true;
  }

  m_CheckedPaths.insert(pathIdx);
  uint32_t endNode = path[path.size() - 1];
  if(endNode == PATH_END)
    return false;

  // Check any paths linked to by the end node of the current path
  const rdcarray<uint32_t> &childPathsToCheck = m_BlockPathLinks[endNode];
  for(uint32_t childPathIdx : childPathsToCheck)
  {
    if(m_CheckedPaths.count(childPathIdx) != 0)
      continue;

    m_CheckedPaths.insert(childPathIdx);
    const BlockPath &childPath = m_Paths[childPathIdx];
    uint32_t childPartStartIdx = ~0U;
    for(uint32_t i = 0; i < childPath.size(); ++i)
    {
      if(childPath[i] == endNode)
      {
        childPartStartIdx = i;
        break;
      }
    }
    RDCASSERTNOTEQUAL(childPartStartIdx, ~0U);
    if(!BlockInAllPaths(block, childPathIdx, childPartStartIdx))
      return false;
  }
  return true;
}

void ControlFlow::FindUniformBlocks(rdcarray<uint32_t> &uniformBlocks)
{
  // 3. Find Uniform Blocks
  for(uint32_t b : m_Blocks)
    m_BlockPathLinks[b].clear();

  // Generate a list of path indexes for each block in the paths
  for(uint32_t pathIdx = 0; pathIdx < m_Paths.size(); ++pathIdx)
  {
    for(uint32_t block : m_Paths[pathIdx])
    {
      if(block == PATH_END)
        break;
      m_BlockPathLinks[block].push_back(pathIdx);
    }
  }

  // For each path that does not end with PATH_END
  // append the child path nodes which only have a single path and are not already in the path
  // This is an optimisation to reduce the amount of recursion required when tracing paths
  // In particular to help the speed of BlockInAnyPath()
  for(uint32_t p = 0; p < m_Paths.size(); ++p)
  {
    BlockPath &path = m_Paths[p];
    uint32_t endNode = path[path.size() - 1];
    while(endNode != PATH_END)
    {
      const rdcarray<uint32_t> &gotos = m_BlockLinks.at(endNode);
      if(gotos.size() > 1)
        break;
      endNode = gotos[0];
      if(path.contains(endNode))
        break;
      path.push_back(endNode);
    }
  }

  rdcarray<uint32_t> loopBlocks;
  // A loop block is defined by any block which appears in any path starting from the block
  for(uint32_t block : m_Blocks)
  {
    bool loop = false;
    for(uint32_t pathIdx = 0; pathIdx < m_Paths.size(); ++pathIdx)
    {
      m_CheckedPaths.clear();
      uint32_t startIdx = ~0U;
      for(uint32_t i = 0; i < m_Paths[pathIdx].size() - 1; ++i)
      {
        if(m_Paths[pathIdx][i] == block)
        {
          startIdx = i;
          break;
        }
      }
      // BlockInAllPaths will also check all paths linked to from the end node of the path
      if(startIdx != ~0U && BlockInAnyPath(block, pathIdx, startIdx + 1))
      {
        loop = true;
        break;
      }
    }
    if(loop)
      loopBlocks.push_back(block);
  }

  rdcarray<uint32_t> allPathsBlocks;

  // An all paths block is defined by any block which appears in all paths
  // all paths includes walking any paths linked at the end node of the path being walked
  for(uint32_t block : m_Blocks)
  {
    bool uniform = true;
    for(uint32_t pathIdx = 0; pathIdx < m_Paths.size(); ++pathIdx)
    {
      m_CheckedPaths.clear();
      // BlockInAllPaths will also check all paths linked to from the end node of the path
      if(!BlockInAllPaths(block, pathIdx, 0))
      {
        uniform = false;
        break;
      }
    }
    if(uniform)
      allPathsBlocks.push_back(block);
  }

  // A uniform block is defined as an all paths block which is not part of a loop
  uniformBlocks.clear();
  for(uint32_t block : allPathsBlocks)
  {
    if(!loopBlocks.contains(block))
      uniformBlocks.push_back(block);
  }
}

void FindUniformBlocks(const rdcarray<BlockLink> &links, rdcarray<uint32_t> &uniformBlocks)
{
  ControlFlow controlFlow(links);

  controlFlow.FindUniformBlocks(uniformBlocks);
}

};    // namespace DXIL

#if ENABLED(ENABLE_UNIT_TESTS)

#include <limits>
#include "catch/catch.hpp"

using namespace DXIL;

TEST_CASE("DXIL Control Flow", "[dxil]")
{
  SECTION("FindUniformBlocks")
  {
    rdcarray<uint32_t> outputs;
    {
      // Degenerate case
      rdcarray<BlockLink> inputs;
      DXIL::FindUniformBlocks(inputs, outputs);
      REQUIRE(0 == outputs.count());
    }
    {
      // Only uniform flow is the start and end
      // 0 -> 1
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      DXIL::FindUniformBlocks(inputs, outputs);
      REQUIRE(2 == outputs.count());
      REQUIRE(outputs.contains(0U));
      REQUIRE(outputs.contains(1U));
    }

    {
      // Single uniform flow between start and end
      // 0 -> 1 -> 3
      // 0 -> 2 -> 3
      // 3 -> 4
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 3});
      inputs.push_back({0, 2});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      DXIL::FindUniformBlocks(inputs, outputs);
      REQUIRE(3 == outputs.count());
      REQUIRE(outputs.contains(0U));
      REQUIRE(outputs.contains(3U));
      REQUIRE(outputs.contains(4U));
    }

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
      DXIL::FindUniformBlocks(inputs, outputs);
      REQUIRE(3 == outputs.count());
      REQUIRE(outputs.contains(0U));
      REQUIRE(outputs.contains(2U));
      REQUIRE(outputs.contains(4U));
    }
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
      DXIL::FindUniformBlocks(inputs, outputs);
      REQUIRE(2 == outputs.count());
      REQUIRE(outputs.contains(0U));
      REQUIRE(outputs.contains(6U));
    }
    {
      // Finite loop (3 -> 4 -> 5 -> 3)
      rdcarray<BlockLink> inputs;
      inputs.push_back({0, 1});
      inputs.push_back({1, 2});
      inputs.push_back({0, 2});
      inputs.push_back({5, 3});
      inputs.push_back({2, 3});
      inputs.push_back({3, 4});
      inputs.push_back({4, 5});
      inputs.push_back({3, 5});
      inputs.push_back({5, 6});
      DXIL::FindUniformBlocks(inputs, outputs);
      REQUIRE(3 == outputs.count());
      REQUIRE(outputs.contains(0U));
      REQUIRE(outputs.contains(2U));
      REQUIRE(outputs.contains(6U));
    }

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
      DXIL::FindUniformBlocks(inputs, outputs);
      REQUIRE(2 == outputs.count());
      REQUIRE(outputs.contains(0U));
      REQUIRE(outputs.contains(6U));
    }

    {
      // Complex case with multiple loops
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
      DXIL::FindUniformBlocks(inputs, outputs);
      REQUIRE(8 == outputs.count());
      REQUIRE(outputs.contains(0U));
      REQUIRE(outputs.contains(3U));
      REQUIRE(outputs.contains(5U));
      REQUIRE(outputs.contains(11U));
      REQUIRE(outputs.contains(17U));
      REQUIRE(outputs.contains(19U));
      REQUIRE(outputs.contains(21U));
      REQUIRE(outputs.contains(26U));
    }
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
