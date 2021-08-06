/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Baldur Karlsson
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

#include "arm_counters.h"

ARMCounters::ARMCounters()
{
}

ARMCounters::~ARMCounters()
{
}

bool ARMCounters::Init()
{
  return false;
}

rdcarray<GPUCounter> ARMCounters::GetPublicCounterIds()
{
  return {};
}

CounterDescription ARMCounters::GetCounterDescription(GPUCounter index)
{
  return {};
}

void ARMCounters::EnableCounter(GPUCounter counter)
{
}

void ARMCounters::DisableAllCounters()
{
}

uint32_t ARMCounters::GetPassCount()
{
  return 0;
}

void ARMCounters::BeginPass(uint32_t passID)
{
}

void ARMCounters::EndPass()
{
}

void ARMCounters::BeginSample(uint32_t eventId)
{
}

void ARMCounters::EndSample()
{
}

rdcarray<CounterResult> ARMCounters::GetCounterData(const rdcarray<uint32_t> &eventIDs,
                                                    const rdcarray<GPUCounter> &counters)
{
  return {};
}
