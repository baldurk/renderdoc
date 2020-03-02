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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "hwc_names.hpp"

#ifndef DOXYGEN_SKIP_THIS

#	if defined(ANDROID) || defined(__ANDROID__)
/* We use _IOR_BAD/_IOW_BAD rather than _IOR/_IOW otherwise fails to compile with NDK-BUILD because of _IOC_TYPECHECK is defined, not because the paramter is invalid */
#		define MALI_IOR(a, b, c) _IOR_BAD(a, b, c)
#		define MALI_IOW(a, b, c) _IOW_BAD(a, b, c)
#	else
#		define MALI_IOR(a, b, c) _IOR(a, b, c)
#		define MALI_IOW(a, b, c) _IOW(a, b, c)
#	endif

namespace mali_userspace
{
union uk_header
{
	uint32_t id;
	uint32_t ret;
	uint64_t sizer;
};

#	define BASE_GPU_NUM_TEXTURE_FEATURES_REGISTERS 3
#	define BASE_MAX_COHERENT_GROUPS 16

struct mali_base_gpu_core_props
{
	uint32_t product_id;
	uint16_t version_status;
	uint16_t minor_revision;
	uint16_t major_revision;
	uint16_t padding;
	uint32_t gpu_speed_mhz;
	uint32_t gpu_freq_khz_max;
	uint32_t gpu_freq_khz_min;
	uint32_t log2_program_counter_size;
	uint32_t texture_features[BASE_GPU_NUM_TEXTURE_FEATURES_REGISTERS];
	uint64_t gpu_available_memory_size;
};

struct mali_base_gpu_l2_cache_props
{
	uint8_t log2_line_size;
	uint8_t log2_cache_size;
	uint8_t num_l2_slices;
	uint8_t padding[5];
};

struct mali_base_gpu_tiler_props
{
	uint32_t bin_size_bytes;
	uint32_t max_active_levels;
};

struct mali_base_gpu_thread_props
{
	uint32_t max_threads;
	uint32_t max_workgroup_size;
	uint32_t max_barrier_size;
	uint16_t max_registers;
	uint8_t  max_task_queue;
	uint8_t  max_thread_group_split;
	uint8_t  impl_tech;
	uint8_t  padding[7];
};

struct mali_base_gpu_coherent_group
{
	uint64_t core_mask;
	uint16_t num_cores;
	uint16_t padding[3];
};

struct mali_base_gpu_coherent_group_info
{
	uint32_t                     num_groups;
	uint32_t                     num_core_groups;
	uint32_t                     coherency;
	uint32_t                     padding;
	mali_base_gpu_coherent_group group[BASE_MAX_COHERENT_GROUPS];
};

#	define GPU_MAX_JOB_SLOTS 16
struct gpu_raw_gpu_props
{
	uint64_t shader_present;
	uint64_t tiler_present;
	uint64_t l2_present;
	uint64_t unused_1;

	uint32_t l2_features;
	uint32_t suspend_size;
	uint32_t mem_features;
	uint32_t mmu_features;

	uint32_t as_present;

	uint32_t js_present;
	uint32_t js_features[GPU_MAX_JOB_SLOTS];
	uint32_t tiler_features;
	uint32_t texture_features[3];

	uint32_t gpu_id;

	uint32_t thread_max_threads;
	uint32_t thread_max_workgroup_size;
	uint32_t thread_max_barrier_size;
	uint32_t thread_features;

	uint32_t coherency_mode;
};

struct mali_base_gpu_props
{
	mali_base_gpu_core_props          core_props;
	mali_base_gpu_l2_cache_props      l2_props;
	uint64_t                          unused;
	mali_base_gpu_tiler_props         tiler_props;
	mali_base_gpu_thread_props        thread_props;
	gpu_raw_gpu_props                 raw_props;
	mali_base_gpu_coherent_group_info coherency_info;
};

struct kbase_uk_gpuprops
{
	uk_header           header;
	mali_base_gpu_props props;
};

#	define KBASE_GPUPROP_VALUE_SIZE_U8 (0x0)
#	define KBASE_GPUPROP_VALUE_SIZE_U16 (0x1)
#	define KBASE_GPUPROP_VALUE_SIZE_U32 (0x2)
#	define KBASE_GPUPROP_VALUE_SIZE_U64 (0x3)

#	define KBASE_GPUPROP_PRODUCT_ID 1
#	define KBASE_GPUPROP_MINOR_REVISION 3
#	define KBASE_GPUPROP_MAJOR_REVISION 4

#	define KBASE_GPUPROP_COHERENCY_NUM_GROUPS 61
#	define KBASE_GPUPROP_COHERENCY_NUM_CORE_GROUPS 62
#	define KBASE_GPUPROP_COHERENCY_GROUP_0 64
#	define KBASE_GPUPROP_COHERENCY_GROUP_1 65
#	define KBASE_GPUPROP_COHERENCY_GROUP_2 66
#	define KBASE_GPUPROP_COHERENCY_GROUP_3 67
#	define KBASE_GPUPROP_COHERENCY_GROUP_4 68
#	define KBASE_GPUPROP_COHERENCY_GROUP_5 69
#	define KBASE_GPUPROP_COHERENCY_GROUP_6 70
#	define KBASE_GPUPROP_COHERENCY_GROUP_7 71
#	define KBASE_GPUPROP_COHERENCY_GROUP_8 72
#	define KBASE_GPUPROP_COHERENCY_GROUP_9 73
#	define KBASE_GPUPROP_COHERENCY_GROUP_10 74
#	define KBASE_GPUPROP_COHERENCY_GROUP_11 75
#	define KBASE_GPUPROP_COHERENCY_GROUP_12 76
#	define KBASE_GPUPROP_COHERENCY_GROUP_13 77
#	define KBASE_GPUPROP_COHERENCY_GROUP_14 78
#	define KBASE_GPUPROP_COHERENCY_GROUP_15 79

#	define KBASE_GPUPROP_L2_NUM_L2_SLICES 15

struct gpu_props
{
	uint32_t product_id;
	uint16_t minor_revision;
	uint16_t major_revision;
	uint32_t num_groups;
	uint32_t num_core_groups;
	uint64_t core_mask[16];

	uint32_t l2_slices;
};

static const struct
{
	uint32_t type;
	size_t   offset;
	int      size;
} gpu_property_mapping[] = {
#	define PROP(name, member)                                        \
		{                                                             \
			KBASE_GPUPROP_##name, offsetof(struct gpu_props, member), \
			    sizeof(((struct gpu_props *) 0)->member)              \
		}
    PROP(PRODUCT_ID, product_id),
    PROP(MINOR_REVISION, minor_revision),
    PROP(MAJOR_REVISION, major_revision),
    PROP(COHERENCY_NUM_GROUPS, num_groups),
    PROP(COHERENCY_NUM_CORE_GROUPS, num_core_groups),
    PROP(COHERENCY_GROUP_0, core_mask[0]),
    PROP(COHERENCY_GROUP_1, core_mask[1]),
    PROP(COHERENCY_GROUP_2, core_mask[2]),
    PROP(COHERENCY_GROUP_3, core_mask[3]),
    PROP(COHERENCY_GROUP_4, core_mask[4]),
    PROP(COHERENCY_GROUP_5, core_mask[5]),
    PROP(COHERENCY_GROUP_6, core_mask[6]),
    PROP(COHERENCY_GROUP_7, core_mask[7]),
    PROP(COHERENCY_GROUP_8, core_mask[8]),
    PROP(COHERENCY_GROUP_9, core_mask[9]),
    PROP(COHERENCY_GROUP_10, core_mask[10]),
    PROP(COHERENCY_GROUP_11, core_mask[11]),
    PROP(COHERENCY_GROUP_12, core_mask[12]),
    PROP(COHERENCY_GROUP_13, core_mask[13]),
    PROP(COHERENCY_GROUP_14, core_mask[14]),
    PROP(COHERENCY_GROUP_15, core_mask[15]),

    PROP(L2_NUM_L2_SLICES, l2_slices),
#	undef PROP
    {0, 0, 0}};

struct kbase_hwcnt_reader_metadata
{
	uint64_t timestamp  = 0;
	uint32_t event_id   = 0;
	uint32_t buffer_idx = 0;
};

namespace
{
/** Message header */
union kbase_uk_hwcnt_header
{
	/* 32-bit number identifying the UK function to be called. */
	uint32_t id;
	/* The int return code returned by the called UK function. */
	uint32_t ret;
	/* Used to ensure 64-bit alignment of this union. Do not remove. */
	uint64_t sizer;
};

/** IOCTL parameters to check version */
struct kbase_uk_hwcnt_reader_version_check_args
{
	union kbase_uk_hwcnt_header header;

	uint16_t major;
	uint16_t minor;
	uint8_t  padding[4];
};

union kbase_pointer
{
	void *   value;
	uint32_t compat_value;
	uint64_t sizer;
};

struct kbase_ioctl_get_gpuprops
{
	kbase_pointer buffer;
	uint32_t      size;
	uint32_t      flags;
};

struct kbase_ioctl_version_check
{
	uint16_t major;
	uint16_t minor;
};

struct kbase_ioctl_set_flags
{
	uint32_t create_flags;
};

struct kbase_ioctl_hwcnt_reader_setup
{
	uint32_t buffer_count;
	uint32_t jm_bm;
	uint32_t shader_bm;
	uint32_t tiler_bm;
	uint32_t mmu_l2_bm;
};

#	define KBASE_IOCTL_TYPE 0x80
#	define KBASE_IOCTL_GET_GPUPROPS MALI_IOW(KBASE_IOCTL_TYPE, 3, struct mali_userspace::kbase_ioctl_get_gpuprops)
#	define KBASE_IOCTL_VERSION_CHECK _IOWR(KBASE_IOCTL_TYPE, 0, struct mali_userspace::kbase_ioctl_version_check)
#	define KBASE_IOCTL_SET_FLAGS _IOW(KBASE_IOCTL_TYPE, 1, struct mali_userspace::kbase_ioctl_set_flags)
#	define KBASE_IOCTL_HWCNT_READER_SETUP _IOW(KBASE_IOCTL_TYPE, 8, struct mali_userspace::kbase_ioctl_hwcnt_reader_setup)

/** IOCTL parameters to set flags */
struct kbase_uk_hwcnt_reader_set_flags
{
	union kbase_uk_hwcnt_header header;

	uint32_t create_flags;
	uint32_t padding;
};

/** IOCTL parameters to configure reader */
struct kbase_uk_hwcnt_reader_setup
{
	union kbase_uk_hwcnt_header header;

	/* IN */
	uint32_t buffer_count;
	uint32_t jm_bm;
	uint32_t shader_bm;
	uint32_t tiler_bm;
	uint32_t mmu_l2_bm;

	/* OUT */
	int32_t fd;
};

static const uint32_t HWCNT_READER_API = 1;

struct uku_version_check_args
{
	uk_header header;
	uint16_t  major;
	uint16_t  minor;
	uint8_t   padding[4];
};

enum
{
	UKP_FUNC_ID_CHECK_VERSION = 0,
	/* Related to mali0 ioctl interface */
	LINUX_UK_BASE_MAGIC              = 0x80,
	BASE_CONTEXT_CREATE_KERNEL_FLAGS = 0x2,
	KBASE_FUNC_HWCNT_UK_FUNC_ID      = 512,
	KBASE_FUNC_GPU_PROPS_REG_DUMP    = KBASE_FUNC_HWCNT_UK_FUNC_ID + 14,
	KBASE_FUNC_HWCNT_READER_SETUP    = KBASE_FUNC_HWCNT_UK_FUNC_ID + 36,
	KBASE_FUNC_HWCNT_DUMP            = KBASE_FUNC_HWCNT_UK_FUNC_ID + 11,
	KBASE_FUNC_HWCNT_CLEAR           = KBASE_FUNC_HWCNT_UK_FUNC_ID + 12,
	KBASE_FUNC_SET_FLAGS             = KBASE_FUNC_HWCNT_UK_FUNC_ID + 18,

	/* The ids of ioctl commands for the reader interface */
	KBASE_HWCNT_READER                 = 0xBE,
	KBASE_HWCNT_READER_GET_HWVER       = MALI_IOR(KBASE_HWCNT_READER, 0x00, uint32_t),
	KBASE_HWCNT_READER_GET_BUFFER_SIZE = MALI_IOR(KBASE_HWCNT_READER, 0x01, uint32_t),
	KBASE_HWCNT_READER_DUMP            = MALI_IOW(KBASE_HWCNT_READER, 0x10, uint32_t),
	KBASE_HWCNT_READER_CLEAR           = MALI_IOW(KBASE_HWCNT_READER, 0x11, uint32_t),
	KBASE_HWCNT_READER_GET_BUFFER      = MALI_IOR(KBASE_HWCNT_READER, 0x20, struct kbase_hwcnt_reader_metadata),
	KBASE_HWCNT_READER_PUT_BUFFER      = MALI_IOW(KBASE_HWCNT_READER, 0x21, struct kbase_hwcnt_reader_metadata),
	KBASE_HWCNT_READER_SET_INTERVAL    = MALI_IOW(KBASE_HWCNT_READER, 0x30, uint32_t),
	KBASE_HWCNT_READER_ENABLE_EVENT    = MALI_IOW(KBASE_HWCNT_READER, 0x40, uint32_t),
	KBASE_HWCNT_READER_DISABLE_EVENT   = MALI_IOW(KBASE_HWCNT_READER, 0x41, uint32_t),
	KBASE_HWCNT_READER_GET_API_VERSION = MALI_IOW(KBASE_HWCNT_READER, 0xFF, uint32_t)
};

enum
{
	PIPE_DESCRIPTOR_IN,  /**< The index of a pipe's input descriptor. */
	PIPE_DESCRIPTOR_OUT, /**< The index of a pipe's output descriptor. */

	PIPE_DESCRIPTOR_COUNT /**< The number of descriptors forming a pipe. */
};

enum
{
	POLL_DESCRIPTOR_SIGNAL,       /**< The index of the signal descriptor in poll fds array. */
	POLL_DESCRIPTOR_HWCNT_READER, /**< The index of the hwcnt reader descriptor in poll fds array. */

	POLL_DESCRIPTOR_COUNT /**< The number of descriptors poll is waiting for. */
};

/** Write a single byte into the pipe to interrupt the reader thread */
typedef char poll_data_t;
}        // namespace

template <typename T>
static inline int mali_ioctl(int fd, T &arg)
{
	auto *    hdr = &arg.header;
	const int cmd = _IOC(_IOC_READ | _IOC_WRITE, LINUX_UK_BASE_MAGIC, hdr->id, sizeof(T));

	if (ioctl(fd, cmd, &arg))
		return -1;
	if (hdr->ret)
		return -1;

	return 0;
}
}        // namespace mali_userspace

#endif /* DOXYGEN_SKIP_THIS */
