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

#include "value.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace hwcpipe
{
// The available CPU counters. Profiler implementations will support a subset of them.
enum class CpuCounter
{
	Cycles,
	Instructions,
	CacheReferences,
	CacheMisses,
	BranchInstructions,
	BranchMisses,

	L1Accesses,
	InstrRetired,
	L2Accesses,
	L3Accesses,
	BusReads,
	BusWrites,
	MemReads,
	MemWrites,
	ASESpec,
	VFPSpec,
	CryptoSpec,

	MaxValue
};

// Mapping from CPU counter names to enum values. Used for JSON initialization.
const std::unordered_map<std::string, CpuCounter> cpu_counter_names {
	{"Cycles", CpuCounter::Cycles},
	{"Instructions", CpuCounter::Instructions},
	{"CacheReferences", CpuCounter::CacheReferences},
	{"CacheMisses", CpuCounter::CacheMisses},
	{"BranchInstructions", CpuCounter::BranchInstructions},
	{"BranchMisses", CpuCounter::BranchMisses},

	{"L1Accesses", CpuCounter::L1Accesses},
	{"InstrRetired", CpuCounter::InstrRetired},
	{"L2Accesses", CpuCounter::L2Accesses},
	{"L3Accesses", CpuCounter::L3Accesses},
	{"BusReads", CpuCounter::BusReads},
	{"BusWrites", CpuCounter::BusWrites},
	{"MemReads", CpuCounter::MemReads},
	{"MemWrites", CpuCounter::MemWrites},
	{"ASESpec", CpuCounter::ASESpec},
	{"VFPSpec", CpuCounter::VFPSpec},
	{"CryptoSpec", CpuCounter::CryptoSpec},
};

// A hash function for CpuCounter values
struct CpuCounterHash
{
	template <typename T>
	std::size_t operator()(T t) const
	{
		return static_cast<std::size_t>(t);
	}
};

struct CpuCounterInfo
{
	std::string desc;
	std::string unit;
};

// Mapping from each counter to its corresponding information (description and unit)
const std::unordered_map<CpuCounter, CpuCounterInfo, CpuCounterHash> cpu_counter_info {
	{CpuCounter::Cycles, {"Number of CPU cycles", "cycles"}},
	{CpuCounter::Instructions, {"Number of CPU instructions", "instructions"}},
	{CpuCounter::CacheReferences, {"Number of cache references", "references"}},
	{CpuCounter::CacheMisses, {"Number of cache misses", "misses"}},
	{CpuCounter::BranchInstructions, {"Number of branch instructions", "instructions"}},
	{CpuCounter::BranchMisses, {"Number of branch misses", "misses"}},

	{CpuCounter::L1Accesses, {"L1 data cache accesses", "accesses"}},
	{CpuCounter::InstrRetired, {"All retired instructions", "instructions"}},
	{CpuCounter::L2Accesses, {"L2 data cache accesses", "accesses"}},
	{CpuCounter::L3Accesses, {"L3 data cache accesses", "accesses"}},
	{CpuCounter::BusReads, {"Bus access reads", "beats"}},
	{CpuCounter::BusWrites, {"Bus access writes", "beats"}},
	{CpuCounter::MemReads, {"Data memory access, load instructions", "instructions"}},
	{CpuCounter::MemWrites, {"Data memory access, store instructions", "instructions"}},
	{CpuCounter::ASESpec, {"Speculatively executed SIMD operations", "operations"}},
	{CpuCounter::VFPSpec, {"Speculatively executed floating point operations", "operations"}},
	{CpuCounter::CryptoSpec, {"Speculatively executed cryptographic operations", "operations"}},
};

typedef std::unordered_set<CpuCounter, CpuCounterHash> CpuCounterSet;
typedef std::unordered_map<CpuCounter, Value, CpuCounterHash>
	CpuMeasurements;

/** An interface for classes that collect CPU performance data. */
class CpuProfiler
{
public:
	virtual ~CpuProfiler() = default;

	// Returns the enabled counters
	virtual const CpuCounterSet &enabled_counters() const = 0;

	// Returns the counters that the platform supports
	virtual const CpuCounterSet &supported_counters() const = 0;

	// Sets the enabled counters after initialization
	virtual void set_enabled_counters(CpuCounterSet counters) = 0;

	// Starts a profiling session
	virtual void run() = 0;

	// Sample the counters. Returns a map of measurements for the counters
	// that are both available and enabled.
	// A profiling session must be running when sampling the counters.
	virtual const CpuMeasurements &sample() = 0;

	// Stops the active profiling session
	virtual void stop() = 0;
};

}        // namespace hwcpipe
