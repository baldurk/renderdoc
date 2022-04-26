/*
 * Copyright (c) 2019-2022 ARM Limited.
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

#include "gpu_profiler.h"

#include "hwc.hpp"

#include <functional>
#include <vector>

namespace hwcpipe
{
/** A Gpu profiler that uses Mali counter data. */
class MaliProfiler : public GpuProfiler
{
public:
	explicit MaliProfiler(const GpuCounterSet &enabled_counters);
	virtual ~MaliProfiler() = default;

	virtual const GpuCounterSet &enabled_counters() const override
	{
		return enabled_counters_;
	}

	virtual const GpuCounterSet &supported_counters() const override
	{
		return supported_counters_;
	};

	virtual void set_enabled_counters(GpuCounterSet counters) override
	{
		enabled_counters_ = std::move(counters);
	};

	virtual void                   run() override;
	virtual const GpuMeasurements &sample() override;
	virtual void                   stop() override;

private:
	GpuCounterSet enabled_counters_ {};

	const GpuCounterSet supported_counters_ {
		GpuCounter::GpuCycles,
		GpuCounter::VertexCycles,
		GpuCounter::ComputeCycles,
		GpuCounter::VertexComputeCycles,
		GpuCounter::FragmentCycles,
		GpuCounter::TilerCycles,
		GpuCounter::VertexJobs,
		GpuCounter::ComputeJobs,
		GpuCounter::VertexComputeJobs,
		GpuCounter::FragmentJobs,
		GpuCounter::Pixels,

		GpuCounter::CulledPrimitives,
		GpuCounter::VisiblePrimitives,
		GpuCounter::InputPrimitives,

		GpuCounter::Tiles,
		GpuCounter::TransactionEliminations,

		GpuCounter::EarlyZTests,
		GpuCounter::EarlyZKilled,
		GpuCounter::LateZTests,
		GpuCounter::LateZKilled,

		GpuCounter::Instructions,
		GpuCounter::DivergedInstructions,

		GpuCounter::ShaderFragmentCycles,
		GpuCounter::ShaderComputeCycles,
		GpuCounter::ShaderCycles,
		GpuCounter::ShaderArithmeticCycles,
		GpuCounter::ShaderInterpolatorCycles,
		GpuCounter::ShaderLoadStoreCycles,
		GpuCounter::ShaderTextureCycles,

		GpuCounter::CacheReadLookups,
		GpuCounter::CacheWriteLookups,
		GpuCounter::ExternalMemoryReadAccesses,
		GpuCounter::ExternalMemoryWriteAccesses,
		GpuCounter::ExternalMemoryReadStalls,
		GpuCounter::ExternalMemoryWriteStalls,
		GpuCounter::ExternalMemoryReadBytes,
		GpuCounter::ExternalMemoryWriteBytes,
	};

	typedef std::function<double(void)>                             MaliValueGetter;
	std::unordered_map<GpuCounter, MaliValueGetter, GpuCounterHash> mappings_ {};

	const char *const         device_ {"/dev/mali0"};
	int                       num_cores_ {0};
	int                       num_l2_slices_ {0};
	int                       gpu_id_ {0};
	uint32_t                  hw_ver_ {0};
	int                       buffer_count_ {16};
	size_t                    buffer_size_ {0};
	uint8_t *                 sample_data_ {nullptr};
	uint64_t                  timestamp_ {0};
	const char *const *       names_lut_ {nullptr};
	std::vector<uint32_t>     raw_counter_buffer_ {};
	std::vector<unsigned int> core_index_remap_ {};
	int                       fd_ {-1};
	int                       hwc_fd_ {-1};

	GpuMeasurements measurements_ {};

	void            init();
	void            sample_counters();
	void            wait_next_event();
	const uint32_t *get_counters(mali_userspace::MaliCounterBlockName block, int index = 0) const;
	uint64_t        get_counter_value(mali_userspace::MaliCounterBlockName block, const char *name) const;
	int             find_counter_index_by_name(mali_userspace::MaliCounterBlockName block, const char *name) const;
};

}        // namespace hwcpipe
