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

#include "pmu_counter.h"

#include <asm/unistd.h>
#include <cstring>
#include <linux/version.h>
#include <stdexcept>
#include <sys/ioctl.h>

PmuCounter::PmuCounter() :
	_perf_config()
{
	_perf_config.size = sizeof(perf_event_attr);

	// Start disabled
	_perf_config.disabled = 1;
	// The inherit bit specifies that this counter should count events of child
	// tasks as well as the task specified
	_perf_config.inherit = 1;
	// Enables saving of event counts on context switch for inherited tasks
	_perf_config.inherit_stat = 1;
}

PmuCounter::PmuCounter(PmuEventInfo config) :
	PmuCounter()
{
	open(config);
}

PmuCounter::~PmuCounter()
{
	close();
}

void PmuCounter::open(PmuEventInfo config)
{
	_perf_config.config = config.event;
	_perf_config.type   = config.type;
	open(_perf_config);
}

void PmuCounter::open(const perf_event_attr &perf_config)
{
	// Measure this process/thread (+ children) on any CPU
	_fd = syscall(__NR_perf_event_open, &perf_config, 0, -1, -1, 0);

	if (_fd < 0)
	{
		throw std::runtime_error("perf_event_open failed. Counter ID: " + config_to_str(_perf_config));
	}

	const int result = ioctl(_fd, PERF_EVENT_IOC_ENABLE, 0);
	if (result == -1)
	{
		throw std::runtime_error("Failed to enable PMU counter: " + std::string(strerror(errno)));
	}
}

void PmuCounter::close()
{
	if (_fd != -1)
	{
		::close(_fd);
		_fd = -1;
	}
}

bool PmuCounter::reset()
{
	const int result = ioctl(_fd, PERF_EVENT_IOC_RESET, 0);

	if (result == -1)
	{
		throw std::runtime_error("Failed to reset PMU counter: " + std::string(std::strerror(errno)));
	}

	return result != -1;
}

std::string PmuCounter::config_to_str(const perf_event_attr &perf_config)
{
	switch (perf_config.type)
	{
		case PERF_TYPE_HARDWARE:
			switch (perf_config.config)
			{
				case PERF_COUNT_HW_CPU_CYCLES:
					return "PERF_COUNT_HW_CPU_CYCLES";
				case PERF_COUNT_HW_INSTRUCTIONS:
					return "PERF_COUNT_HW_INSTRUCTIONS";
				case PERF_COUNT_HW_CACHE_REFERENCES:
					return "PERF_COUNT_HW_CACHE_REFERENCES";
				case PERF_COUNT_HW_CACHE_MISSES:
					return "PERF_COUNT_HW_CACHE_MISSES";
				case PERF_COUNT_HW_BRANCH_INSTRUCTIONS:
					return "PERF_COUNT_HW_BRANCH_INSTRUCTIONS";
				case PERF_COUNT_HW_BRANCH_MISSES:
					return "PERF_COUNT_HW_BRANCH_MISSES";
				case PERF_COUNT_HW_BUS_CYCLES:
					return "PERF_COUNT_HW_BUS_CYCLES";
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
				case PERF_COUNT_HW_STALLED_CYCLES_FRONTEND:
					return "PERF_COUNT_HW_STALLED_CYCLES_FRONTEND";
				case PERF_COUNT_HW_STALLED_CYCLES_BACKEND:
					return "PERF_COUNT_HW_STALLED_CYCLES_BACKEND";
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
				case PERF_COUNT_HW_REF_CPU_CYCLES:
					return "PERF_COUNT_HW_REF_CPU_CYCLES";
#endif
				default:
					return "UNKNOWN HARDWARE COUNTER";
			}

		case PERF_TYPE_SOFTWARE:
			switch (perf_config.config)
			{
				case PERF_COUNT_SW_CPU_CLOCK:
					return "PERF_COUNT_SW_CPU_CLOCK";
				case PERF_COUNT_SW_TASK_CLOCK:
					return "PERF_COUNT_SW_TASK_CLOCK";
				case PERF_COUNT_SW_PAGE_FAULTS:
					return "PERF_COUNT_SW_PAGE_FAULTS";
				case PERF_COUNT_SW_CONTEXT_SWITCHES:
					return "PERF_COUNT_SW_CONTEXT_SWITCHES";
				case PERF_COUNT_SW_CPU_MIGRATIONS:
					return "PERF_COUNT_SW_CPU_MIGRATIONS";
				case PERF_COUNT_SW_PAGE_FAULTS_MIN:
					return "PERF_COUNT_SW_PAGE_FAULTS_MIN";
				case PERF_COUNT_SW_PAGE_FAULTS_MAJ:
					return "PERF_COUNT_SW_PAGE_FAULTS_MAJ";
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)
				case PERF_COUNT_SW_ALIGNMENT_FAULTS:
					return "PERF_COUNT_SW_ALIGNMENT_FAULTS";
				case PERF_COUNT_SW_EMULATION_FAULTS:
					return "PERF_COUNT_SW_EMULATION_FAULTS";
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
				case PERF_COUNT_SW_DUMMY:
					return "PERF_COUNT_SW_DUMMY";
#endif
				default:
					return "UNKNOWN SOFTWARE COUNTER";
			}

		case PERF_TYPE_RAW:
			switch (static_cast<PmuImplDefined>(perf_config.config))
			{
				case PmuImplDefined::L1_ACCESSES:
					return "L1_ACCESSES";
				case PmuImplDefined::INSTR_RETIRED:
					return "INSTR_RETIRED";
				case PmuImplDefined::L2_ACCESSES:
					return "L2_ACCESSES";
				case PmuImplDefined::L3_ACCESSES:
					return "L3_ACCESSES";
				case PmuImplDefined::BUS_READS:
					return "BUS_READS";
				case PmuImplDefined::BUS_WRITES:
					return "BUS_WRITES";
				case PmuImplDefined::MEM_READS:
					return "MEM_READS";
				case PmuImplDefined::MEM_WRITES:
					return "MEM_WRITES";
				case PmuImplDefined::ASE_SPEC:
					return "ASE_SPEC";
				case PmuImplDefined::VFP_SPEC:
					return "VFP_SPEC";
				case PmuImplDefined::CRYPTO_SPEC:
					return "CRYPTO_SPEC";
				default:
					return "UNKNOWN RAW COUNTER";
			}

		default:
			return std::to_string(perf_config.config);
	}
}
