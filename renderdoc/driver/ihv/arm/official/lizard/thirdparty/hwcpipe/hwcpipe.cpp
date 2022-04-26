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

#include "hwcpipe.h"
#include "hwcpipe_log.h"

#ifdef __linux__
	#include "vendor/arm/pmu/pmu_profiler.h"
	#include "vendor/arm/mali/mali_profiler.h"
#endif

#ifndef HWCPIPE_NO_JSON
	#include <json.hpp>
using json = nlohmann::json;
#endif

#include <memory>

namespace hwcpipe
{
#ifndef HWCPIPE_NO_JSON
HWCPipe::HWCPipe(const char *json_string)
{
	auto json = json::parse(json_string);

	CpuCounterSet enabled_cpu_counters {};
	auto          cpu = json.find("cpu");
	if (cpu != json.end())
	{
		for (auto &counter_name : cpu->items())
		{
			auto counter = cpu_counter_names.find(counter_name.value().get<std::string>());
			if (counter != cpu_counter_names.end())
			{
				enabled_cpu_counters.insert(counter->second);
			}
			else
			{
				HWCPIPE_LOG("CPU counter \"%s\" not found.", counter_name.value().get<std::string>().c_str());
			}
		}
	}

	GpuCounterSet enabled_gpu_counters {};
	auto          gpu = json.find("gpu");
	if (gpu != json.end())
	{
		for (auto &counter_name : gpu->items())
		{
			auto counter = gpu_counter_names.find(counter_name.value().get<std::string>());
			if (counter != gpu_counter_names.end())
			{
				enabled_gpu_counters.insert(counter->second);
			}
			else
			{
				HWCPIPE_LOG("GPU counter \"%s\" not found.", counter_name.value().get<std::string>().c_str());
			}
		}
	}

	create_profilers(std::move(enabled_cpu_counters), std::move(enabled_gpu_counters));
}
#endif

HWCPipe::HWCPipe(CpuCounterSet enabled_cpu_counters, GpuCounterSet enabled_gpu_counters)
{
	create_profilers(std::move(enabled_cpu_counters), std::move(enabled_gpu_counters));
}

HWCPipe::HWCPipe()
{
	CpuCounterSet enabled_cpu_counters {
		CpuCounter::Cycles,
		CpuCounter::Instructions,
		CpuCounter::CacheReferences,
		CpuCounter::CacheMisses,
		CpuCounter::BranchInstructions,
		CpuCounter::BranchMisses,
	};

	GpuCounterSet enabled_gpu_counters {
		GpuCounter::GpuCycles,
		GpuCounter::VertexComputeCycles,
		GpuCounter::FragmentCycles,
		GpuCounter::TilerCycles,
		GpuCounter::CacheReadLookups,
		GpuCounter::CacheWriteLookups,
		GpuCounter::ExternalMemoryReadAccesses,
		GpuCounter::ExternalMemoryWriteAccesses,
		GpuCounter::ExternalMemoryReadStalls,
		GpuCounter::ExternalMemoryWriteStalls,
		GpuCounter::ExternalMemoryReadBytes,
		GpuCounter::ExternalMemoryWriteBytes,
	};

	create_profilers(std::move(enabled_cpu_counters), std::move(enabled_gpu_counters));
}

void HWCPipe::set_enabled_cpu_counters(CpuCounterSet counters)
{
	if (cpu_profiler_)
	{
		cpu_profiler_->set_enabled_counters(std::move(counters));
	}
}

void HWCPipe::set_enabled_gpu_counters(GpuCounterSet counters)
{
	if (gpu_profiler_)
	{
		gpu_profiler_->set_enabled_counters(std::move(counters));
	}
}

void HWCPipe::run()
{
	if (cpu_profiler_)
	{
		cpu_profiler_->run();
	}
	if (gpu_profiler_)
	{
		gpu_profiler_->run();
	}
}

Measurements HWCPipe::sample()
{
	Measurements m;
	if (cpu_profiler_)
	{
		m.cpu = &cpu_profiler_->sample();
	}
	if (gpu_profiler_)
	{
		m.gpu = &gpu_profiler_->sample();
	}
	return m;
}

void HWCPipe::stop()
{
	if (cpu_profiler_)
	{
		cpu_profiler_->stop();
	}
	if (gpu_profiler_)
	{
		gpu_profiler_->stop();
	}
}

void HWCPipe::create_profilers(CpuCounterSet enabled_cpu_counters, GpuCounterSet enabled_gpu_counters)
{
	// Automated platform detection
#ifdef __linux__
	try
	{
		if (enabled_cpu_counters.size() != 0)
		{
			cpu_profiler_ = std::unique_ptr<PmuProfiler>(new PmuProfiler(enabled_cpu_counters));
		}
	}
	catch (const std::runtime_error &e)
	{
		HWCPIPE_LOG("PMU profiler initialization failed: %s", e.what());
	}

	try
	{
		if (enabled_gpu_counters.size() != 0)
		{
			gpu_profiler_ = std::unique_ptr<MaliProfiler>(new MaliProfiler(enabled_gpu_counters));
		}
	}
	catch (const std::runtime_error &e)
	{
		HWCPIPE_LOG("Mali profiler initialization failed: %s", e.what());
	}
#else
	HWCPIPE_LOG("No counters available for this platform.");
#endif
}

}        // namespace hwcpipe
