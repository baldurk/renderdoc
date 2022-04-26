/*
 * Copyright (c) 2017-2019 ARM Limited.
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

#include "pmu_profiler.h"

#include "hwcpipe_log.h"

namespace hwcpipe
{
const std::unordered_map<CpuCounter, PmuEventInfo, CpuCounterHash> pmu_mappings {
	{CpuCounter::Cycles, {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES}},
	{CpuCounter::Instructions, {PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS}},
	{CpuCounter::CacheReferences, {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES}},
	{CpuCounter::CacheMisses, {PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES}},
	{CpuCounter::BranchInstructions, {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS}},
	{CpuCounter::BranchMisses, {PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES}},

	{CpuCounter::L1Accesses, {PERF_TYPE_RAW, PmuImplDefined::L1_ACCESSES}},
	{CpuCounter::InstrRetired, {PERF_TYPE_RAW, PmuImplDefined::INSTR_RETIRED}},
	{CpuCounter::L2Accesses, {PERF_TYPE_RAW, PmuImplDefined::L2_ACCESSES}},
	{CpuCounter::L3Accesses, {PERF_TYPE_RAW, PmuImplDefined::L3_ACCESSES}},
	{CpuCounter::BusReads, {PERF_TYPE_RAW, PmuImplDefined::BUS_READS}},
	{CpuCounter::BusWrites, {PERF_TYPE_RAW, PmuImplDefined::BUS_WRITES}},
	{CpuCounter::MemReads, {PERF_TYPE_RAW, PmuImplDefined::MEM_READS}},
	{CpuCounter::MemWrites, {PERF_TYPE_RAW, PmuImplDefined::MEM_WRITES}},
	{CpuCounter::ASESpec, {PERF_TYPE_RAW, PmuImplDefined::ASE_SPEC}},
	{CpuCounter::VFPSpec, {PERF_TYPE_RAW, PmuImplDefined::VFP_SPEC}},
	{CpuCounter::CryptoSpec, {PERF_TYPE_RAW, PmuImplDefined::CRYPTO_SPEC}},
};

PmuProfiler::PmuProfiler(const CpuCounterSet &enabled_counters) :
	enabled_counters_(enabled_counters)
{
	// Set up PMU counters
	for (const auto &counter : enabled_counters)
	{
		const auto &pmu_config = pmu_mappings.find(counter);
		if (pmu_config != pmu_mappings.end())
		{
			try
			{
				// Create a PMU counter with the specified configuration
				auto pmu_counter_res = pmu_counters_.emplace(counter, pmu_config->second);

				// Try reading a value from the counter to check that it opened correctly
				auto &pmu_counter = pmu_counter_res.first->second;
				pmu_counter.get_value<long long>();

				// PMU counter is created and can retrieve values
				available_counters_.insert(counter);
			}
			catch (const std::runtime_error &e)
			{
				// PMU counter initialization failed
				HWCPIPE_LOG("%s", e.what());
			}
		}
	}

	if (available_counters_.size() == 0)
	{
		throw std::runtime_error("PMU counters not available.");
	}
}

void PmuProfiler::run()
{
	for (auto &pmu_counter : pmu_counters_)
	{
		pmu_counter.second.reset();
		prev_measurements_[pmu_counter.first] = Value {};
	}
}

const CpuMeasurements &PmuProfiler::sample()
{
	for (const auto &counter : enabled_counters_)
	{
		const auto &pmu_counter = pmu_counters_.find(counter);
		if (pmu_counter == pmu_counters_.end())
		{
			continue;
		}

		try
		{
			auto value = pmu_counter->second.get_value<long long>();

			// Resetting the PMU counter every frame seems to alter the data,
			// so we make a differential reading.
			measurements_[pmu_counter->first]      = value - prev_measurements_[pmu_counter->first].get<long long>();
			prev_measurements_[pmu_counter->first] = value;
		}
		catch (const std::runtime_error &e)
		{
			HWCPIPE_LOG("Failed to get value from PMU: %s.", e.what());
		}
	}

	return measurements_;
}

void PmuProfiler::stop()
{
	// We don't need to do anything on stop()
}

}        // namespace hwcpipe
