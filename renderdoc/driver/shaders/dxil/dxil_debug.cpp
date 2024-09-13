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

#include "dxil_debug.h"

using namespace DXIL;
using namespace DXDebug;

namespace DXILDebug
{

// static helper function
rdcstr Debugger::GetResourceReferenceName(const DXIL::Program *program,
                                          DXIL::ResourceClass resClass, const BindingSlot &slot)
{
  RDCASSERT(program);
  for(const ResourceReference &resRef : program->m_ResourceReferences)
  {
    if(resRef.resourceBase.resClass != resClass)
      continue;
    if(resRef.resourceBase.space != slot.registerSpace)
      continue;
    if(resRef.resourceBase.regBase > slot.shaderRegister)
      continue;
    if(resRef.resourceBase.regBase + resRef.resourceBase.regCount < slot.shaderRegister)
      continue;

    return program->GetHandleAlias(resRef.handleID);
  }
  RDCERR("Failed to find DXIL %s Resource Space %d Register %d", ToStr(resClass).c_str(),
         slot.registerSpace, slot.shaderRegister);
  return "UNKNOWN_RESOURCE_HANDLE";
}

};    // namespace DXILDebug
