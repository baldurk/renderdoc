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

#pragma once

#include "common/common.h"
#include "driver/shaders/dxbc/dxbc_container.h"
#include "dxil_bytecode.h"

namespace DXILDebug
{
typedef rdcstr Id;
class Debugger;
struct GlobalState;
struct BindingSlot
{
  BindingSlot() : shaderRegister(UINT32_MAX), registerSpace(UINT32_MAX) {}
  BindingSlot(uint32_t shaderReg, uint32_t regSpace)
      : shaderRegister(shaderReg), registerSpace(regSpace)
  {
  }
  BindingSlot(const DXIL::ResourceReference &resRef)
      : shaderRegister(resRef.resourceBase.regBase), registerSpace(resRef.resourceBase.space)
  {
  }

  bool operator<(const BindingSlot &o) const
  {
    if(registerSpace != o.registerSpace)
      return registerSpace < o.registerSpace;
    return shaderRegister < o.shaderRegister;
  }

  uint32_t shaderRegister;
  uint32_t registerSpace;
};

class DebugAPIWrapper
{
};

struct ThreadState
{
};

struct GlobalState
{
  GlobalState() = default;
};

class Debugger : public DXBCContainerDebugger
{
public:
  Debugger() : DXBCContainerDebugger(true){};
};

};    // namespace DXILDebug
