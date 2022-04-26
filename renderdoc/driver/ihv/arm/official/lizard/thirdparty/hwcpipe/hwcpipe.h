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

/* Force disable JSON in HWCPIPE */
#define HWCPIPE_NO_JSON

#include "cpu_profiler.h"
#include "gpu_profiler.h"

#include <functional>
#include <memory>

namespace hwcpipe
{
struct Measurements
{
	const CpuMeasurements *cpu {nullptr};
	const GpuMeasurements *gpu {nullptr};
};

/** A class that collects CPU/GPU performance data. */
class HWCPipe
{
public:
#ifndef HWCPIPE_NO_JSON
	// Initializes HWCPipe via a JSON configuration string
	explicit HWCPipe(const char *json_string);
#endif

	// Initializes HWCPipe with the specified counters
	HWCPipe(CpuCounterSet enabled_cpu_counters, GpuCounterSet enabled_gpu_counters);

	// Initializes HWCPipe with a default set of counters
	HWCPipe();

	// Sets the enabled counters for the CPU profiler
	void set_enabled_cpu_counters(CpuCounterSet counters);

	// Sets the enabled counters for the GPU profiler
	void set_enabled_gpu_counters(GpuCounterSet counters);

	// Starts a profiling session
	void run();

	// Sample the counters. The function returns pointers to the CPU and GPU
	// measurements maps, if the corresponding profiler is enabled.
	// The entries in the maps are the counters that are both available and enabled.
	// A profiling session must be running when sampling the counters.
	Measurements sample();

	// Stops the active profiling session
	void stop();

	const CpuProfiler *cpu_profiler()
	{
		return cpu_profiler_.get();
	}
	const GpuProfiler *gpu_profiler()
	{
		return gpu_profiler_.get();
	}

private:
	std::unique_ptr<CpuProfiler> cpu_profiler_ {};
	std::unique_ptr<GpuProfiler> gpu_profiler_ {};

	void create_profilers(CpuCounterSet enabled_cpu_counters, GpuCounterSet enabled_gpu_counters);
};

}        // namespace hwcpipe
