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
// The available GPU counters. Profiler implementations will support a subset of them.
enum class GpuCounter
{
	GpuCycles,
	VertexComputeCycles,
	FragmentCycles,
	TilerCycles,

	VertexComputeJobs,
	FragmentJobs,
	Pixels,

	Tiles,
	TransactionEliminations,
	EarlyZTests,
	EarlyZKilled,
	LateZTests,
	LateZKilled,

	Instructions,
	DivergedInstructions,

	ShaderCycles,
	ShaderArithmeticCycles,
	ShaderLoadStoreCycles,
	ShaderTextureCycles,

	CacheReadLookups,
	CacheWriteLookups,
	ExternalMemoryReadAccesses,
	ExternalMemoryWriteAccesses,
	ExternalMemoryReadStalls,
	ExternalMemoryWriteStalls,
	ExternalMemoryReadBytes,
	ExternalMemoryWriteBytes,

	MaxValue
};

// Mapping from GPU counter names to enum values. Used for JSON initialization.
const std::unordered_map<std::string, GpuCounter> gpu_counter_names{
    {"GpuCycles", GpuCounter::GpuCycles},
    {"VertexComputeCycles", GpuCounter::VertexComputeCycles},
    {"FragmentCycles", GpuCounter::FragmentCycles},
    {"TilerCycles", GpuCounter::TilerCycles},

    {"VertexComputeJobs", GpuCounter::VertexComputeJobs},
    {"Tiles", GpuCounter::Tiles},
    {"TransactionEliminations", GpuCounter::TransactionEliminations},
    {"FragmentJobs", GpuCounter::FragmentJobs},
    {"Pixels", GpuCounter::Pixels},

    {"EarlyZTests", GpuCounter::EarlyZTests},
    {"EarlyZKilled", GpuCounter::EarlyZKilled},
    {"LateZTests", GpuCounter::LateZTests},
    {"LateZKilled", GpuCounter::LateZKilled},

    {"Instructions", GpuCounter::Instructions},
    {"DivergedInstructions", GpuCounter::DivergedInstructions},

    {"ShaderCycles", GpuCounter::ShaderCycles},
    {"ShaderArithmeticCycles", GpuCounter::ShaderArithmeticCycles},
    {"ShaderLoadStoreCycles", GpuCounter::ShaderLoadStoreCycles},
    {"ShaderTextureCycles", GpuCounter::ShaderTextureCycles},

    {"CacheReadLookups", GpuCounter::CacheReadLookups},
    {"CacheWriteLookups", GpuCounter::CacheWriteLookups},
    {"ExternalMemoryReadAccesses", GpuCounter::ExternalMemoryReadAccesses},
    {"ExternalMemoryWriteAccesses", GpuCounter::ExternalMemoryWriteAccesses},
    {"ExternalMemoryReadStalls", GpuCounter::ExternalMemoryReadStalls},
    {"ExternalMemoryWriteStalls", GpuCounter::ExternalMemoryWriteStalls},
    {"ExternalMemoryReadBytes", GpuCounter::ExternalMemoryReadBytes},
    {"ExternalMemoryWriteBytes", GpuCounter::ExternalMemoryWriteBytes},
};

// A hash function for GpuCounter values
struct GpuCounterHash
{
	template <typename T>
	std::size_t operator()(T t) const
	{
		return static_cast<std::size_t>(t);
	}
};

struct GpuCounterInfo
{
	std::string desc;
	std::string unit;
};

// Mapping from each counter to its corresponding information (description and unit)
const std::unordered_map<GpuCounter, GpuCounterInfo, GpuCounterHash> gpu_counter_info{
    {GpuCounter::GpuCycles, {"Number of GPU cycles", "cycles"}},
    {GpuCounter::VertexComputeCycles, {"Number of vertex/compute cycles", "cycles"}},
    {GpuCounter::FragmentCycles, {"Number of fragment cycles", "cycles"}},
    {GpuCounter::TilerCycles, {"Number of tiler cycles", "cycles"}},

    {GpuCounter::VertexComputeJobs, {"Number of vertex/compute jobs", "jobs"}},
    {GpuCounter::Tiles, {"Number of physical tiles written", "tiles"}},
    {GpuCounter::TransactionEliminations, {"Number of transaction eliminations", "tiles"}},
    {GpuCounter::FragmentJobs, {"Number of fragment jobs", "jobs"}},
    {GpuCounter::Pixels, {"Number of pixels shaded", "cycles"}},

    {GpuCounter::EarlyZTests, {"Early-Z tests performed", "tests"}},
    {GpuCounter::EarlyZKilled, {"Early-Z tests resulting in a kill", "tests"}},
    {GpuCounter::LateZTests, {"Late-Z tests performed", "tests"}},
    {GpuCounter::LateZKilled, {"Late-Z tests resulting in a kill", "tests"}},

    {GpuCounter::Instructions, {"Number of shader instructions", "instructions"}},
    {GpuCounter::DivergedInstructions, {"Number of diverged shader instructions", "instructions"}},

    {GpuCounter::ShaderCycles, {"Shader total cycles", "cycles"}},
    {GpuCounter::ShaderArithmeticCycles, {"Shader arithmetic cycles", "cycles"}},
    {GpuCounter::ShaderLoadStoreCycles, {"Shader load/store cycles", "cycles"}},
    {GpuCounter::ShaderTextureCycles, {"Shader texture cycles", "cycles"}},

    {GpuCounter::CacheReadLookups, {"Cache read lookups", "lookups"}},
    {GpuCounter::CacheWriteLookups, {"Cache write lookups", "lookups"}},
    {GpuCounter::ExternalMemoryReadAccesses, {"Reads from external memory", "accesses"}},
    {GpuCounter::ExternalMemoryWriteAccesses, {"Writes to external memory", "accesses"}},
    {GpuCounter::ExternalMemoryReadStalls, {"Stalls when reading from external memory", "stalls"}},
    {GpuCounter::ExternalMemoryWriteStalls, {"Stalls when writing to external memory", "stalls"}},
    {GpuCounter::ExternalMemoryReadBytes, {"Bytes read to external memory", "B"}},
    {GpuCounter::ExternalMemoryWriteBytes, {"Bytes written to external memory", "B"}},
};

typedef std::unordered_set<GpuCounter, GpuCounterHash>        GpuCounterSet;
typedef std::unordered_map<GpuCounter, Value, GpuCounterHash> GpuMeasurements;

/** An interface for classes that collect GPU performance data. */
class GpuProfiler
{
  public:
	virtual ~GpuProfiler() = default;

	// Returns the enabled counters
	virtual const GpuCounterSet &enabled_counters() const = 0;

	// Returns the counters that the platform supports
	virtual const GpuCounterSet &supported_counters() const = 0;

	// Sets the enabled counters after initialization
	virtual void set_enabled_counters(GpuCounterSet counters) = 0;

	// Starts a profiling session
	virtual void run() = 0;

	// Sample the counters. Returns a map of measurements for the counters
	// that are both available and enabled.
	// A profiling session must be running when sampling the counters.
	virtual const GpuMeasurements &sample() = 0;

	// Stops the active profiling session
	virtual void stop() = 0;
};

}        // namespace hwcpipe
