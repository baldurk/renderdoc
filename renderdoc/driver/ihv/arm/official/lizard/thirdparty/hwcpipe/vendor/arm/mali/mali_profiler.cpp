/*
 * Copyright (c) 2017-2022 ARM Limited.
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
#include "mali_profiler.h"

#include "hwcpipe_log.h"

#include <algorithm>
#include <stdexcept>

using mali_userspace::MALI_NAME_BLOCK_JM;
using mali_userspace::MALI_NAME_BLOCK_MMU;
using mali_userspace::MALI_NAME_BLOCK_SHADER;
using mali_userspace::MALI_NAME_BLOCK_TILER;

namespace hwcpipe
{
namespace
{
struct MaliHWInfo
{
	unsigned mp_count;
	unsigned gpu_id;
	unsigned r_value;
	unsigned p_value;
	unsigned core_mask;
	unsigned l2_slices;
};

MaliHWInfo get_mali_hw_info(const char *path)
{
	int fd = open(path, O_RDWR);        // NOLINT

	if (fd < 0)
	{
		throw std::runtime_error("Failed to get HW info.");
	}

	{
		// Try matching Job Manager version IOCTL
		bool checked_version = true;
		mali_userspace::kbase_uk_hwcnt_reader_version_check_args version_check_args;
		version_check_args.header.id = mali_userspace::UKP_FUNC_ID_CHECK_VERSION_JM;
		version_check_args.major     = 10;
		version_check_args.minor     = 2;

		if (mali_userspace::mali_ioctl(fd, version_check_args) != 0)
		{
			mali_userspace::kbase_ioctl_version_check _version_check_args = {0, 0};
			if (ioctl(fd, KBASE_IOCTL_VERSION_CHECK_JM, &_version_check_args) < 0)
			{
				checked_version = false;
			}
		}

		// Try matching CSF version IOCTL
		if (!checked_version)
		{
			mali_userspace::kbase_uk_hwcnt_reader_version_check_args version_check_args;
			version_check_args.header.id = mali_userspace::UKP_FUNC_ID_CHECK_VERSION_CSF;
			version_check_args.major     = 1;
			version_check_args.minor     = 4;

			if (mali_userspace::mali_ioctl(fd, version_check_args) != 0)
			{
				mali_userspace::kbase_ioctl_version_check _version_check_args = {0, 0};
				if (ioctl(fd, KBASE_IOCTL_VERSION_CHECK_CSF, &_version_check_args) < 0)
				{
					close(fd);
					throw std::runtime_error("Failed to check version.");
				}
			}
		}
	}

	{
		mali_userspace::kbase_uk_hwcnt_reader_set_flags flags;        // NOLINT
		memset(&flags, 0, sizeof(flags));
		flags.header.id    = mali_userspace::KBASE_FUNC_SET_FLAGS;        // NOLINT
		flags.create_flags = mali_userspace::BASE_CONTEXT_CREATE_KERNEL_FLAGS;

		if (mali_userspace::mali_ioctl(fd, flags) != 0)
		{
			mali_userspace::kbase_ioctl_set_flags _flags = {1u << 1};
			if (ioctl(fd, KBASE_IOCTL_SET_FLAGS, &_flags) < 0)
			{
				close(fd);
				throw std::runtime_error("Failed settings flags ioctl.");
			}
		}
	}

	{
		MaliHWInfo hw_info;        // NOLINT
		memset(&hw_info, 0, sizeof(hw_info));
		mali_userspace::kbase_uk_gpuprops props = {};
		props.header.id                         = mali_userspace::KBASE_FUNC_GPU_PROPS_REG_DUMP;
		if (mali_ioctl(fd, props) == 0)
		{
			hw_info.gpu_id  = props.props.core_props.product_id;
			hw_info.r_value = props.props.core_props.major_revision;
			hw_info.p_value = props.props.core_props.minor_revision;
			for (uint32_t i = 0; i < props.props.coherency_info.num_core_groups; i++)
				hw_info.core_mask |= props.props.coherency_info.group[i].core_mask;
			hw_info.mp_count  = __builtin_popcountll(hw_info.core_mask);
			hw_info.l2_slices = props.props.l2_props.num_l2_slices;

			close(fd);
		}
		else
		{
			mali_userspace::kbase_ioctl_get_gpuprops get_props = {};
			int                                      ret;
			if ((ret = ioctl(fd, KBASE_IOCTL_GET_GPUPROPS, &get_props)) < 0)
			{
				throw std::runtime_error("Failed getting GPU properties.");
				close(fd);
			}

			get_props.size = ret;
			std::vector<uint8_t> buffer(ret);
			get_props.buffer.value = buffer.data();
			ret                    = ioctl(fd, KBASE_IOCTL_GET_GPUPROPS, &get_props);
			if (ret < 0)
			{
				throw std::runtime_error("Failed getting GPU properties.");
				close(fd);
			}

#define READ_U8(p) ((p)[0])
#define READ_U16(p) (READ_U8((p)) | (uint16_t(READ_U8((p) + 1)) << 8))
#define READ_U32(p) (READ_U16((p)) | (uint32_t(READ_U16((p) + 2)) << 16))
#define READ_U64(p) (READ_U32((p)) | (uint64_t(READ_U32((p) + 4)) << 32))

			mali_userspace::gpu_props props = {};

			const auto *ptr  = buffer.data();
			int         size = ret;
			while (size > 0)
			{
				uint32_t type       = READ_U32(ptr);
				uint32_t value_type = type & 3;
				uint64_t value;

				ptr += 4;
				size -= 4;

				switch (value_type)
				{
					case KBASE_GPUPROP_VALUE_SIZE_U8:
						value = READ_U8(ptr);
						ptr++;
						size--;
						break;
					case KBASE_GPUPROP_VALUE_SIZE_U16:
						value = READ_U16(ptr);
						ptr += 2;
						size -= 2;
						break;
					case KBASE_GPUPROP_VALUE_SIZE_U32:
						value = READ_U32(ptr);
						ptr += 4;
						size -= 4;
						break;
					case KBASE_GPUPROP_VALUE_SIZE_U64:
						value = READ_U64(ptr);
						ptr += 8;
						size -= 8;
						break;
				}

				for (unsigned i = 0; mali_userspace::gpu_property_mapping[i].type; i++)
				{
					if (mali_userspace::gpu_property_mapping[i].type == (type >> 2))
					{
						auto  offset = mali_userspace::gpu_property_mapping[i].offset;
						void *p      = reinterpret_cast<uint8_t *>(&props) + offset;
						switch (mali_userspace::gpu_property_mapping[i].size)
						{
							case 1:
								*reinterpret_cast<uint8_t *>(p) = value;
								break;
							case 2:
								*reinterpret_cast<uint16_t *>(p) = value;
								break;
							case 4:
								*reinterpret_cast<uint32_t *>(p) = value;
								break;
							case 8:
								*reinterpret_cast<uint64_t *>(p) = value;
								break;
							default:
								throw std::runtime_error("Invalid property size.");
								close(fd);
						}
						break;
					}
				}
			}

			hw_info.gpu_id  = props.product_id;
			hw_info.r_value = props.major_revision;
			hw_info.p_value = props.minor_revision;
			for (uint32_t i = 0; i < props.num_core_groups; i++)
				hw_info.core_mask |= props.core_mask[i];
			hw_info.mp_count  = __builtin_popcountll(hw_info.core_mask);
			hw_info.l2_slices = props.l2_slices;

			close(fd);
		}

		return hw_info;
	}
}
}        // namespace

typedef std::function<uint64_t(void)> MaliValueGetter;

MaliProfiler::MaliProfiler(const GpuCounterSet &enabled_counters) :
	enabled_counters_(enabled_counters)
{
	// Throws if setup fails
	init();

	const std::unordered_map<GpuCounter, MaliValueGetter, GpuCounterHash> valhall_csf_mappings = {
		{GpuCounter::GpuCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "GPU_ACTIVE"); }},
		{GpuCounter::ComputeCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "ITER_COMP_ACTIVE"); }},
		{GpuCounter::VertexCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "ITER_TILER_ACTIVE"); }},
		{GpuCounter::FragmentCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "ITER_FRAGMENT_ACTIVE"); }},
		{GpuCounter::TilerCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "TILER_ACTIVE"); }},

		{GpuCounter::ComputeJobs, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "ITER_COMP_JOB_COMPLETED"); }},
		{GpuCounter::VertexJobs, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "ITER_TILER_JOB_COMPLETED"); }},
		{GpuCounter::FragmentJobs, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "ITER_FRAG_JOB_COMPLETED"); }},
		{GpuCounter::Pixels, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "ITER_FRAG_TASK_COMPLETED") * 1024; }},

		{GpuCounter::CulledPrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_CULLED") + get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_CLIPPED") + get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_SAT_CULLED"); }},
		{GpuCounter::VisiblePrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_VISIBLE"); }},
		{GpuCounter::InputPrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "TRIANGLES") + get_counter_value(MALI_NAME_BLOCK_TILER, "LINES") + get_counter_value(MALI_NAME_BLOCK_TILER, "POINTS"); }},

		{GpuCounter::Tiles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_PTILES"); }},
		{GpuCounter::TransactionEliminations, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_TRANS_ELIM"); }},
		{GpuCounter::EarlyZTests, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_QUADS_EZS_TEST"); }},
		{GpuCounter::EarlyZKilled, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_QUADS_EZS_KILL"); }},
		{GpuCounter::LateZTests, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_LZS_TEST"); }},
		{GpuCounter::LateZKilled, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_LZS_KILL"); }},

		{GpuCounter::Instructions, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_FMA") + get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_CVT") + get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_SFU") + get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_MSG"); }},
		{GpuCounter::DivergedInstructions, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_DIVERGED"); }},

		{GpuCounter::ShaderComputeCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "COMPUTE_ACTIVE"); }},
		{GpuCounter::ShaderFragmentCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_ACTIVE"); }},
		{GpuCounter::ShaderCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_CORE_ACTIVE"); }},
		// The three units run in parallel so we can approximate cycles by taking the largest value. SFU instructions use 4 cycles per warp.
		{GpuCounter::ShaderArithmeticCycles, [this] { return std::max(get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_FMA"), std::max(get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_CVT"), 4 * get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_SFU"))); }},
		{GpuCounter::ShaderInterpolatorCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "VARY_SLOT_16") + get_counter_value(MALI_NAME_BLOCK_SHADER, "VARY_SLOT_32"); }},
		{GpuCounter::ShaderLoadStoreCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_READ_FULL") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_WRITE_FULL") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_READ_SHORT") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_WRITE_SHORT") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_ATOMIC"); }},
		{GpuCounter::ShaderTextureCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "TEX_FILT_NUM_OPERATIONS"); }},

		{GpuCounter::CacheReadLookups, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_READ_LOOKUP"); }},
		{GpuCounter::CacheWriteLookups, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_WRITE_LOOKUP"); }},
		{GpuCounter::ExternalMemoryReadAccesses, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_READ"); }},
		{GpuCounter::ExternalMemoryWriteAccesses, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_WRITE"); }},
		{GpuCounter::ExternalMemoryReadStalls, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_AR_STALL"); }},
		{GpuCounter::ExternalMemoryWriteStalls, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_W_STALL"); }},
		{GpuCounter::ExternalMemoryReadBytes, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_READ_BEATS") * 16; }},
		{GpuCounter::ExternalMemoryWriteBytes, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_WRITE_BEATS") * 16; }},
	};

	const std::unordered_map<GpuCounter, MaliValueGetter, GpuCounterHash> valhall_mappings = {
		{GpuCounter::GpuCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "GPU_ACTIVE"); }},
		{GpuCounter::VertexComputeCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS1_ACTIVE"); }},
		{GpuCounter::FragmentCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_ACTIVE"); }},
		{GpuCounter::TilerCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "TILER_ACTIVE"); }},

		{GpuCounter::VertexComputeJobs, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS1_JOBS"); }},
		{GpuCounter::FragmentJobs, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_JOBS"); }},
		{GpuCounter::Pixels, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_TASKS") * 1024; }},

		{GpuCounter::CulledPrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_CULLED") + get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_CLIPPED") + get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_SAT_CULLED"); }},
		{GpuCounter::VisiblePrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_VISIBLE"); }},
		{GpuCounter::InputPrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "TRIANGLES") + get_counter_value(MALI_NAME_BLOCK_TILER, "LINES") + get_counter_value(MALI_NAME_BLOCK_TILER, "POINTS"); }},

		{GpuCounter::Tiles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_PTILES"); }},
		{GpuCounter::TransactionEliminations, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_TRANS_ELIM"); }},
		{GpuCounter::EarlyZTests, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_QUADS_EZS_TEST"); }},
		{GpuCounter::EarlyZKilled, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_QUADS_EZS_KILL"); }},
		{GpuCounter::LateZTests, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_LZS_TEST"); }},
		{GpuCounter::LateZKilled, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_LZS_KILL"); }},

		{GpuCounter::Instructions, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_FMA") + get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_CVT") + get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_SFU") + get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_MSG"); }},
		{GpuCounter::DivergedInstructions, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_DIVERGED"); }},

		{GpuCounter::ShaderComputeCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "COMPUTE_ACTIVE"); }},
		{GpuCounter::ShaderFragmentCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_ACTIVE"); }},
		{GpuCounter::ShaderCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_CORE_ACTIVE"); }},
		// The three units run in parallel so we can approximate cycles by taking the largest value. SFU instructions use 4 cycles per warp.
		{GpuCounter::ShaderArithmeticCycles, [this] { return std::max(get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_FMA"), std::max(get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_CVT"), 4 * get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_SFU"))); }},
		{GpuCounter::ShaderInterpolatorCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "VARY_SLOT_16") + get_counter_value(MALI_NAME_BLOCK_SHADER, "VARY_SLOT_32"); }},
		{GpuCounter::ShaderLoadStoreCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_READ_FULL") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_WRITE_FULL") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_READ_SHORT") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_WRITE_SHORT") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_ATOMIC"); }},
		{GpuCounter::ShaderTextureCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "TEX_FILT_NUM_OPERATIONS"); }},

		{GpuCounter::CacheReadLookups, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_READ_LOOKUP"); }},
		{GpuCounter::CacheWriteLookups, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_WRITE_LOOKUP"); }},
		{GpuCounter::ExternalMemoryReadAccesses, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_READ"); }},
		{GpuCounter::ExternalMemoryWriteAccesses, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_WRITE"); }},
		{GpuCounter::ExternalMemoryReadStalls, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_AR_STALL"); }},
		{GpuCounter::ExternalMemoryWriteStalls, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_W_STALL"); }},
		{GpuCounter::ExternalMemoryReadBytes, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_READ_BEATS") * 16; }},
		{GpuCounter::ExternalMemoryWriteBytes, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_WRITE_BEATS") * 16; }},
	};

	const std::unordered_map<GpuCounter, MaliValueGetter, GpuCounterHash> bifrost_mappings = {
		{GpuCounter::GpuCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "GPU_ACTIVE"); }},
		{GpuCounter::VertexComputeCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS1_ACTIVE"); }},
		{GpuCounter::FragmentCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_ACTIVE"); }},
		{GpuCounter::TilerCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "TILER_ACTIVE"); }},

		{GpuCounter::VertexComputeJobs, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS1_JOBS"); }},
		{GpuCounter::FragmentJobs, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_JOBS"); }},
		{GpuCounter::Pixels, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_TASKS") * 1024; }},

		{GpuCounter::CulledPrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_CULLED") + get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_CLIPPED") + get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_SAT_CULLED"); }},
		{GpuCounter::VisiblePrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_VISIBLE"); }},
		{GpuCounter::InputPrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "TRIANGLES") + get_counter_value(MALI_NAME_BLOCK_TILER, "LINES") + get_counter_value(MALI_NAME_BLOCK_TILER, "POINTS"); }},

		{GpuCounter::Tiles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_PTILES"); }},
		{GpuCounter::TransactionEliminations, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_TRANS_ELIM"); }},
		{GpuCounter::EarlyZTests, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_QUADS_EZS_TEST"); }},
		{GpuCounter::EarlyZKilled, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_QUADS_EZS_KILL"); }},
		{GpuCounter::LateZTests, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_LZS_TEST"); }},
		{GpuCounter::LateZKilled, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_LZS_KILL"); }},

		{GpuCounter::Instructions, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_COUNT"); }},
		{GpuCounter::DivergedInstructions, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_DIVERGED"); }},

		{GpuCounter::ShaderComputeCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "COMPUTE_ACTIVE"); }},
		{GpuCounter::ShaderFragmentCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_ACTIVE"); }},
		{GpuCounter::ShaderCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_CORE_ACTIVE"); }},
		{GpuCounter::ShaderArithmeticCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "EXEC_INSTR_COUNT"); }},
		{GpuCounter::ShaderInterpolatorCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "VARY_SLOT_16") + get_counter_value(MALI_NAME_BLOCK_SHADER, "VARY_SLOT_32"); }},
		{GpuCounter::ShaderLoadStoreCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_READ_FULL") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_WRITE_FULL") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_READ_SHORT") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_WRITE_SHORT") + get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_MEM_ATOMIC"); }},
		{GpuCounter::ShaderTextureCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "TEX_FILT_NUM_OPERATIONS"); }},

		{GpuCounter::CacheReadLookups, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_READ_LOOKUP"); }},
		{GpuCounter::CacheWriteLookups, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_WRITE_LOOKUP"); }},
		{GpuCounter::ExternalMemoryReadAccesses, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_READ"); }},
		{GpuCounter::ExternalMemoryWriteAccesses, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_WRITE"); }},
		{GpuCounter::ExternalMemoryReadStalls, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_AR_STALL"); }},
		{GpuCounter::ExternalMemoryWriteStalls, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_W_STALL"); }},
		{GpuCounter::ExternalMemoryReadBytes, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_READ_BEATS") * 16; }},
		{GpuCounter::ExternalMemoryWriteBytes, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_WRITE_BEATS") * 16; }},
	};

	const std::unordered_map<GpuCounter, MaliValueGetter, GpuCounterHash> midgard_mappings = {
		{GpuCounter::GpuCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "GPU_ACTIVE"); }},
		{GpuCounter::VertexComputeCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS1_ACTIVE"); }},
		{GpuCounter::FragmentCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_ACTIVE"); }},

		{GpuCounter::VertexComputeJobs, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS1_JOBS"); }},
		{GpuCounter::FragmentJobs, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_JOBS"); }},
		{GpuCounter::Pixels, [this] { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_TASKS") * 1024; }},

		{GpuCounter::CulledPrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_CULLED") + get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_CLIPPED"); }},
		{GpuCounter::VisiblePrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "PRIM_VISIBLE"); }},
		{GpuCounter::InputPrimitives, [this] { return get_counter_value(MALI_NAME_BLOCK_TILER, "TRIANGLES") + get_counter_value(MALI_NAME_BLOCK_TILER, "LINES") + get_counter_value(MALI_NAME_BLOCK_TILER, "POINTS"); }},

		{GpuCounter::Tiles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_PTILES"); }},
		{GpuCounter::TransactionEliminations, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_TRANS_ELIM"); }},
		{GpuCounter::EarlyZTests, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_QUADS_EZS_TEST"); }},
		{GpuCounter::EarlyZKilled, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_QUADS_EZS_KILLED"); }},
		{GpuCounter::LateZTests, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_THREADS_LZS_TEST"); }},
		{GpuCounter::LateZKilled, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_THREADS_LZS_KILLED"); }},

		{GpuCounter::ShaderComputeCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "COMPUTE_ACTIVE"); }},
		{GpuCounter::ShaderFragmentCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "FRAG_ACTIVE"); }},
		{GpuCounter::ShaderCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "TRIPIPE_ACTIVE"); }},
		{GpuCounter::ShaderArithmeticCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "ARITH_WORDS"); }},
		{GpuCounter::ShaderLoadStoreCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "LS_ISSUES"); }},
		{GpuCounter::ShaderTextureCycles, [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "TEX_ISSUES"); }},

		{GpuCounter::CacheReadLookups, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_READ_LOOKUP"); }},
		{GpuCounter::CacheWriteLookups, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_WRITE_LOOKUP"); }},
		{GpuCounter::ExternalMemoryReadAccesses, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_READ"); }},
		{GpuCounter::ExternalMemoryWriteAccesses, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_WRITE"); }},
		{GpuCounter::ExternalMemoryReadStalls, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_AR_STALL"); }},
		{GpuCounter::ExternalMemoryWriteStalls, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_W_STALL"); }},
		{GpuCounter::ExternalMemoryReadBytes, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_READ_BEATS") * 16; }},
		{GpuCounter::ExternalMemoryWriteBytes, [this] { return get_counter_value(MALI_NAME_BLOCK_MMU, "L2_EXT_WRITE_BEATS") * 16; }},
	};

	auto product = std::find_if(std::begin(mali_userspace::products), std::end(mali_userspace::products), [&](const mali_userspace::CounterMapping &cm) {
		return (cm.product_mask & gpu_id_) == cm.product_id;
	});

	if (product != std::end(mali_userspace::products))
	{
		switch (product->product_id)
		{
			case mali_userspace::PRODUCT_ID_T60X:
			case mali_userspace::PRODUCT_ID_T62X:
			case mali_userspace::PRODUCT_ID_T72X:
				mappings_                     = midgard_mappings;
				mappings_[GpuCounter::Pixels] = [this]() { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_TASKS") * 256; };
				break;
			case mali_userspace::PRODUCT_ID_T76X:
			case mali_userspace::PRODUCT_ID_T82X:
			case mali_userspace::PRODUCT_ID_T83X:
			case mali_userspace::PRODUCT_ID_T86X:
			case mali_userspace::PRODUCT_ID_TFRX:
				mappings_ = midgard_mappings;
				break;
			case mali_userspace::PRODUCT_ID_TMIX:
			case mali_userspace::PRODUCT_ID_THEX:
				mappings_                                  = bifrost_mappings;
				mappings_[GpuCounter::ShaderTextureCycles] = [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "TEX_COORD_ISSUE"); };
				break;
			case mali_userspace::PRODUCT_ID_TSIX:
			case mali_userspace::PRODUCT_ID_TNOX:
			case mali_userspace::PRODUCT_ID_TGOX:
			case mali_userspace::PRODUCT_ID_TDVX:
				mappings_ = bifrost_mappings;
				break;
			case mali_userspace::PRODUCT_ID_TNAXa:
			case mali_userspace::PRODUCT_ID_TNAXb:
			case mali_userspace::PRODUCT_ID_TTRX:
			case mali_userspace::PRODUCT_ID_TOTX:
			case mali_userspace::PRODUCT_ID_TBOX:
			case mali_userspace::PRODUCT_ID_TBOXAE:
				mappings_ = valhall_mappings;
				break;
			case mali_userspace::PRODUCT_ID_TODX:
			case mali_userspace::PRODUCT_ID_TVIX:
			case mali_userspace::PRODUCT_ID_TGRX:
			case mali_userspace::PRODUCT_ID_TVAX:
			default:
				mappings_ = valhall_csf_mappings;
				break;
		}
	}
	else
	{
		HWCPIPE_LOG("Mali counters initialization failed: Failed to identify GPU");
	}
}

void MaliProfiler::init()
{
	MaliHWInfo hw_info = get_mali_hw_info(device_);

	num_cores_     = hw_info.mp_count;
	num_l2_slices_ = hw_info.l2_slices;
	gpu_id_        = hw_info.gpu_id;

	fd_ = open(device_, O_RDWR | O_CLOEXEC | O_NONBLOCK);        // NOLINT

	if (fd_ < 0)
	{
		throw std::runtime_error("Failed to open /dev/mali0.");
	}

	// Set API version
	{
		// Try matching Job Manager version IOCTL
		bool checked_version = true;
		mali_userspace::kbase_uk_hwcnt_reader_version_check_args version_check_args;
		version_check_args.header.id = mali_userspace::UKP_FUNC_ID_CHECK_VERSION_JM;
		version_check_args.major     = 10;
		version_check_args.minor     = 2;

		if (mali_userspace::mali_ioctl(fd_, version_check_args) != 0)
		{
			mali_userspace::kbase_ioctl_version_check _version_check_args = {0, 0};
			if (ioctl(fd_, KBASE_IOCTL_VERSION_CHECK_JM, &_version_check_args) < 0)
			{
				checked_version = false;
			}
		}

		// Try matching CSF version IOCTL
		if (!checked_version)
		{
			mali_userspace::kbase_uk_hwcnt_reader_version_check_args version_check_args;
			version_check_args.header.id = mali_userspace::UKP_FUNC_ID_CHECK_VERSION_CSF;
			version_check_args.major     = 1;
			version_check_args.minor     = 4;

			if (mali_userspace::mali_ioctl(fd_, version_check_args) != 0)
			{
				mali_userspace::kbase_ioctl_version_check _version_check_args = {0, 0};
				if (ioctl(fd_, KBASE_IOCTL_VERSION_CHECK_CSF, &_version_check_args) < 0)
				{
					close(fd_);
					throw std::runtime_error("Failed to check version.");
				}
			}
		}
	}
	
	{
		mali_userspace::kbase_uk_hwcnt_reader_set_flags flags;        // NOLINT
		memset(&flags, 0, sizeof(flags));
		flags.header.id    = mali_userspace::KBASE_FUNC_SET_FLAGS;        // NOLINT
		flags.create_flags = mali_userspace::BASE_CONTEXT_CREATE_KERNEL_FLAGS;

		if (mali_userspace::mali_ioctl(fd_, flags) != 0)
		{
			mali_userspace::kbase_ioctl_set_flags _flags = {1u << 1};
			if (ioctl(fd_, KBASE_IOCTL_SET_FLAGS, &_flags) < 0)
			{
				throw std::runtime_error("Failed settings flags ioctl.");
			}
		}
	}

	{
		mali_userspace::kbase_uk_hwcnt_reader_setup setup;        // NOLINT
		memset(&setup, 0, sizeof(setup));
		setup.header.id    = mali_userspace::KBASE_FUNC_HWCNT_READER_SETUP;        // NOLINT
		setup.buffer_count = buffer_count_;
		setup.jm_bm        = -1;
		setup.shader_bm    = -1;
		setup.tiler_bm     = -1;
		setup.mmu_l2_bm    = -1;
		setup.fd           = -1;

		if (mali_userspace::mali_ioctl(fd_, setup) != 0)
		{
			mali_userspace::kbase_ioctl_hwcnt_reader_setup _setup = {};
			_setup.buffer_count                                   = buffer_count_;
			_setup.jm_bm                                          = -1;
			_setup.shader_bm                                      = -1;
			_setup.tiler_bm                                       = -1;
			_setup.mmu_l2_bm                                      = -1;

			int ret;
			if ((ret = ioctl(fd_, KBASE_IOCTL_HWCNT_READER_SETUP, &_setup)) < 0)
			{
				throw std::runtime_error("Failed setting hwcnt reader ioctl.");
			}
			hwc_fd_ = ret;
		}
		else
		{
			hwc_fd_ = setup.fd;
		}
	}

	{
		uint32_t api_version = ~mali_userspace::HWCNT_READER_API;

		if (ioctl(hwc_fd_, mali_userspace::KBASE_HWCNT_READER_GET_API_VERSION, &api_version) != 0)        // NOLINT
		{
			throw std::runtime_error("Could not determine hwcnt reader API.");
		}
		else if (api_version != mali_userspace::HWCNT_READER_API)
		{
			throw std::runtime_error("Invalid API version.");
		}
	}

	if (ioctl(hwc_fd_, static_cast<int>(mali_userspace::KBASE_HWCNT_READER_GET_BUFFER_SIZE), &buffer_size_) != 0)        // NOLINT
	{
		throw std::runtime_error("Failed to get buffer size.");
	}

	if (ioctl(hwc_fd_, static_cast<int>(mali_userspace::KBASE_HWCNT_READER_GET_HWVER), &hw_ver_) != 0)        // NOLINT
	{
		throw std::runtime_error("Could not determine HW version.");
	}

	if (hw_ver_ < 5)
	{
		throw std::runtime_error("Unsupported HW version.");
	}

	sample_data_ = static_cast<uint8_t *>(mmap(nullptr, buffer_count_ * buffer_size_, PROT_READ, MAP_PRIVATE, hwc_fd_, 0));

	if (sample_data_ == MAP_FAILED)        // NOLINT
	{
		throw std::runtime_error("Failed to map sample data.");
	}

	auto product = std::find_if(std::begin(mali_userspace::products), std::end(mali_userspace::products), [&](const mali_userspace::CounterMapping &cm) {
		return (cm.product_mask & hw_info.gpu_id) == cm.product_id;
	});

	if (product != std::end(mali_userspace::products))
	{
		names_lut_ = product->names_lut;
	}
	else
	{
		throw std::runtime_error("Could not identify GPU.");
	}

	raw_counter_buffer_.resize(buffer_size_ / sizeof(uint32_t));

	// Build core remap table.
	core_index_remap_.clear();
	core_index_remap_.reserve(hw_info.mp_count);

	unsigned int mask = hw_info.core_mask;

	while (mask != 0)
	{
		unsigned int bit = __builtin_ctz(mask);
		core_index_remap_.push_back(bit);
		mask &= ~(1u << bit);
	}
}

void MaliProfiler::run()
{
	sample_counters();
	wait_next_event();
}

void MaliProfiler::stop()
{
	// We don't need to do anything on stop()
}

const GpuMeasurements &MaliProfiler::sample()
{
	sample_counters();
	wait_next_event();

	for (const auto &counter : enabled_counters_)
	{
		auto mapping = mappings_.find(counter);
		if (mapping == mappings_.end())
		{
			continue;
		}

		measurements_[mapping->first] = mapping->second();
	}

	return measurements_;
}

void MaliProfiler::sample_counters()
{
	if (ioctl(hwc_fd_, mali_userspace::KBASE_HWCNT_READER_DUMP, 0) != 0)
	{
		throw std::runtime_error("Could not sample hardware counters.");
	}
}

void MaliProfiler::wait_next_event()
{
	pollfd poll_fd;        // NOLINT
	poll_fd.fd     = hwc_fd_;
	poll_fd.events = POLLIN;

	const int count = poll(&poll_fd, 1, -1);

	if (count < 0)
	{
		throw std::runtime_error("poll() failed.");
	}

	if ((poll_fd.revents & POLLIN) != 0)
	{
		mali_userspace::kbase_hwcnt_reader_metadata meta;        // NOLINT

		if (ioctl(hwc_fd_, static_cast<int>(mali_userspace::KBASE_HWCNT_READER_GET_BUFFER), &meta) != 0)        // NOLINT
		{
			throw std::runtime_error("Failed READER_GET_BUFFER.");
		}

		memcpy(raw_counter_buffer_.data(), sample_data_ + buffer_size_ * meta.buffer_idx, buffer_size_);
		timestamp_ = meta.timestamp;

		if (ioctl(hwc_fd_, mali_userspace::KBASE_HWCNT_READER_PUT_BUFFER, &meta) != 0)        // NOLINT
		{
			throw std::runtime_error("Failed READER_PUT_BUFFER.");
		}
	}
	else if ((poll_fd.revents & POLLHUP) != 0)
	{
		throw std::runtime_error("HWC hung up.");
	}
}

uint64_t MaliProfiler::get_counter_value(mali_userspace::MaliCounterBlockName block, const char *name) const
{
	uint64_t sum = 0;
	switch (block)
	{
		case mali_userspace::MALI_NAME_BLOCK_MMU:
			// If an MMU counter is selected, sum the values over MMU slices
			for (int i = 0; i < num_l2_slices_; i++)
			{
				sum += get_counters(block, i)[find_counter_index_by_name(block, name)];
			}
			return sum;

		case mali_userspace::MALI_NAME_BLOCK_SHADER:
			// If a shader core counter is selected, sum the values over shader cores
			for (int i = 0; i < num_cores_; i++)
			{
				sum += get_counters(block, i)[find_counter_index_by_name(block, name)];
			}
			return sum;

		case mali_userspace::MALI_NAME_BLOCK_JM:
		case mali_userspace::MALI_NAME_BLOCK_TILER:
		default:
			return static_cast<uint64_t>(get_counters(block)[find_counter_index_by_name(block, name)]);
	}
}

const uint32_t *MaliProfiler::get_counters(mali_userspace::MaliCounterBlockName block, int index) const
{
	switch (block)
	{
		case mali_userspace::MALI_NAME_BLOCK_JM:
			return raw_counter_buffer_.data() + mali_userspace::MALI_NAME_BLOCK_SIZE * 0;
		case mali_userspace::MALI_NAME_BLOCK_MMU:
			if (index < 0 || index >= num_l2_slices_)
			{
				throw std::runtime_error("Invalid slice number.");
			}

			// If an MMU counter is selected, index refers to the MMU slice
			return raw_counter_buffer_.data() + mali_userspace::MALI_NAME_BLOCK_SIZE * (2 + index);
		case mali_userspace::MALI_NAME_BLOCK_TILER:
			return raw_counter_buffer_.data() + mali_userspace::MALI_NAME_BLOCK_SIZE * 1;
		default:
			if (index < 0 || index >= num_cores_)
			{
				throw std::runtime_error("Invalid core number.");
			}

			// If a shader core counter is selected, index refers to the core index
			return raw_counter_buffer_.data() + mali_userspace::MALI_NAME_BLOCK_SIZE * (2 + num_l2_slices_ + core_index_remap_[index]);
	}
}

int MaliProfiler::find_counter_index_by_name(mali_userspace::MaliCounterBlockName block, const char *name) const
{
	const char *const *names = &names_lut_[mali_userspace::MALI_NAME_BLOCK_SIZE * block];

	for (int i = 0; i < mali_userspace::MALI_NAME_BLOCK_SIZE; ++i)
	{
		if (strstr(names[i], name) != nullptr)
		{
			return i;
		}
	}

	return -1;
}

}        // namespace hwcpipe
