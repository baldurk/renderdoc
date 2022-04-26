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

#pragma once

#include <cstdint>
#include <cstring>
#include <errno.h>
#include <linux/perf_event.h>
#include <stdexcept>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

#include "hwcpipe_log.h"

enum class PmuImplDefined : uint64_t
{
	L1_ACCESSES   = 0x4,
	INSTR_RETIRED = 0x8,
	L2_ACCESSES   = 0x16,
	L3_ACCESSES   = 0x2b,
	BUS_READS     = 0x60,
	BUS_WRITES    = 0x61,
	MEM_READS     = 0x66,
	MEM_WRITES    = 0x67,
	ASE_SPEC      = 0x74,
	VFP_SPEC      = 0x75,
	CRYPTO_SPEC   = 0x77,
};

struct PmuEventInfo
{
	uint64_t type;
	uint64_t event;

	PmuEventInfo(uint64_t type, uint64_t event) :
		type(type),
		event(event)
	{}

	PmuEventInfo(uint64_t type, PmuImplDefined event) :
		PmuEventInfo(type, static_cast<uint64_t>(event))
	{}
};

/** Class provides access to CPU hardware counters. */
class PmuCounter
{
public:
	/** Default constructor. */
	PmuCounter();

	/** Create PMU counter with specified config.
	 *
	 * This constructor automatically calls @ref open with the default
	 * configuration.
	 *
	 * @param[in] config Counter info.
	 */
	PmuCounter(PmuEventInfo config);

	/** Default destructor. */
	~PmuCounter();

	/** Get the counter value.
	 *
	 * @return Counter value casted to the specified type. */
	template <typename T>
	T get_value() const;

	/** Open the specified counter based on the given configuration.
	 *
	 * @param[in] config The configuration.
	 */
	void open(PmuEventInfo config);

	/** Open the specified configuration.
	 *
	 * @param[in] perf_config The specified configuration.
	 */
	void open(const perf_event_attr &perf_config);

	/** Close the currently open counter. */
	void close();

	/** Reset counter.
	 *
	 * @return false if reset fails. */
	bool reset();

	/** Print counter config ID. */
	std::string config_to_str(const perf_event_attr &perf_config);

private:
	perf_event_attr _perf_config;
	long            _fd {-1};
};

template <typename T>
T PmuCounter::get_value() const
{
	long long     value {};
	const ssize_t result = read(_fd, &value, sizeof(long long));

	if (result == -1)
	{
		throw std::runtime_error("Can't get PMU counter value: " + std::string(std::strerror(errno)));
	}

	return static_cast<T>(value);
}
