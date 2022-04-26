/*
 * Copyright (c) 2019 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "cpu_profiler.h"

#include "pmu_counter.h"

namespace hwcpipe
{
/** A CPU profiler that uses PMU counter data. */
class PmuProfiler : public CpuProfiler
{
public:
	explicit PmuProfiler(const CpuCounterSet &enabled_counters);
	virtual ~PmuProfiler() = default;

	virtual const CpuCounterSet &enabled_counters() const override
	{
		return enabled_counters_;
	}

	virtual const CpuCounterSet &supported_counters() const override
	{
		return supported_counters_;
	};

	virtual void set_enabled_counters(CpuCounterSet counters) override
	{
		enabled_counters_ = std::move(counters);
	};

	virtual void                   run() override;
	virtual const CpuMeasurements &sample() override;
	virtual void                   stop() override;

private:
	CpuCounterSet enabled_counters_ {};
	CpuCounterSet available_counters_ {};

	const CpuCounterSet supported_counters_ {
		CpuCounter::Cycles,
		CpuCounter::Instructions,
		CpuCounter::CacheReferences,
		CpuCounter::CacheMisses,
		CpuCounter::BranchInstructions,
		CpuCounter::BranchMisses,

		CpuCounter::L1Accesses,
		CpuCounter::InstrRetired,
		CpuCounter::L2Accesses,
		CpuCounter::L3Accesses,
		CpuCounter::BusReads,
		CpuCounter::BusWrites,
		CpuCounter::MemReads,
		CpuCounter::MemWrites,
		CpuCounter::ASESpec,
		CpuCounter::VFPSpec,
		CpuCounter::CryptoSpec,
	};

	CpuMeasurements measurements_ {};
	CpuMeasurements prev_measurements_ {};

	std::unordered_map<CpuCounter, PmuCounter, CpuCounterHash> pmu_counters_ {};
};

}        // namespace hwcpipe
