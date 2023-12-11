/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include <chrono>
#include "3rdparty/fmt/core.h"
#include "vk_test.h"

#if defined(SSE_TEST)
#include <immintrin.h>
#endif

struct Alloc
{
  VkDevice device;
  std::string name;
  VkDeviceMemory mem;
  VkMemoryPropertyFlags flags;
  VkBuffer buf;
  uint32_t type;
  VkDeviceSize size;
  byte *data = NULL;

  void map() { vkMapMemory(device, mem, 0, VK_WHOLE_SIZE, 0, (void **)&data); }
  void unmap() { vkUnmapMemory(device, mem); }
};

byte *refData = NULL;
size_t dummyStart, dummyEnd;

namespace FindDiffRange_shipping
{
#if 0
static __m128 zero = {0};
#endif

// assumes a and b both point to 16-byte aligned 16-byte chunks of memory.

// Returns if they're equal or different
bool Vec16NotEqual(void *a, void *b)
{
// disabled SSE version as it's acting dodgy
#if 0
	__m128 avec = _mm_load_ps(aflt);
	__m128 bvec = _mm_load_ps(bflt);

	__m128 diff = _mm_xor_ps(avec, bvec);

	__m128 eq = _mm_cmpeq_ps(diff, zero);
	int mask = _mm_movemask_ps(eq);
	int signMask = _mm_movemask_ps(diff);

	// first check ensures that diff is floatequal to zero (ie. avec bitwise equal to bvec).
	// HOWEVER -0 is floatequal to 0, so we ensure no sign bits are set on diff
	if((mask^0xf) || signMask != 0)
	{
		return true;
	}
	
	return false;
#elif defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) ||      \
    defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__) || \
    (defined(__riscv) && __riscv_xlen == 64)
  uint64_t *a64 = (uint64_t *)a;
  uint64_t *b64 = (uint64_t *)b;

  return a64[0] != b64[0] || a64[1] != b64[1];
#else
  uint32_t *a32 = (uint32_t *)a;
  uint32_t *b32 = (uint32_t *)b;

  return a32[0] != b32[0] || a32[1] != b32[1] || a32[2] != b32[2] || a32[3] != b32[3];
#endif
}

bool FindDiffRange(void *a, void *b, size_t bufSize, size_t &diffStart, size_t &diffEnd)
{
  TEST_ASSERT(uintptr_t(a) % 16 == 0, "misaligned");
  TEST_ASSERT(uintptr_t(b) % 16 == 0, "misaligned");

  diffStart = bufSize + 1;
  diffEnd = 0;

  size_t alignedSize = bufSize & (~0xf);
  size_t numVecs = alignedSize / 16;

  size_t offs = 0;

  float *aflt = (float *)a;
  float *bflt = (float *)b;

  // sweep to find the start of differences
  for(size_t v = 0; v < numVecs; v++)
  {
    if(Vec16NotEqual(aflt, bflt))
    {
      diffStart = offs;
      break;
    }

    aflt += 4;
    bflt += 4;
    offs += 4 * sizeof(float);
  }

  // make sure we're byte-accurate, to comply with WRITE_NO_OVERWRITE
  while(diffStart < bufSize && *((byte *)a + diffStart) == *((byte *)b + diffStart))
    diffStart++;

  // do we have some unaligned bytes at the end of the buffer?
  if(bufSize > alignedSize)
  {
    size_t numBytes = bufSize - alignedSize;

    // if we haven't even found a start, check in these bytes
    if(diffStart > bufSize)
    {
      offs = alignedSize;

      for(size_t by = 0; by < numBytes; by++)
      {
        if(*((byte *)a + alignedSize + by) != *((byte *)b + alignedSize + by))
        {
          diffStart = offs;
          break;
        }

        offs++;
      }
    }

    // sweep from the last byte to find the end
    for(size_t by = 0; by < numBytes; by++)
    {
      if(*((byte *)a + bufSize - 1 - by) != *((byte *)b + bufSize - 1 - by))
      {
        diffEnd = bufSize - by;
        break;
      }
    }
  }

  // if we haven't found a start, or we've found a start AND and end,
  // then we're done.
  if(diffStart > bufSize || diffEnd > 0)
    return diffStart < bufSize;

  offs = alignedSize;

  // sweep from the last __m128
  aflt = (float *)a + offs / sizeof(float) - 4;
  bflt = (float *)b + offs / sizeof(float) - 4;

  for(size_t v = 0; v < numVecs; v++)
  {
    if(Vec16NotEqual(aflt, bflt))
    {
      diffEnd = offs;
      break;
    }

    aflt -= 4;
    bflt -= 4;
    offs -= 16;
  }

  // make sure we're byte-accurate, to comply with WRITE_NO_OVERWRITE
  while(diffEnd > 0 && *((byte *)a + diffEnd - 1) == *((byte *)b + diffEnd - 1))
    diffEnd--;

  // if we found a start then we necessarily found an end
  return diffStart < bufSize;
}
};

void stream_memcpy(void *dst, void *src, size_t len)
{
  char *d = (char *)dst;
  char *s = (char *)src;

#if defined(SSE_TEST)
  /* If dst and src are not co-aligned, or if SSE4.1 is not present, fallback to memcpy(). */
  if(((uintptr_t)d & 15) != ((uintptr_t)s & 15))
  {
    memcpy(d, s, len);
    return;
  }

  /* memcpy() the misaligned header. At the end of this if block, <d> and <s>
   * are aligned to a 16-byte boundary or <len> == 0.
   */
  if((uintptr_t)d & 15)
  {
    uintptr_t bytes_before_alignment_boundary = 16 - ((uintptr_t)d & 15);
    TEST_ASSERT(bytes_before_alignment_boundary < 16, "!");

    memcpy(d, s, std::min(bytes_before_alignment_boundary, len));

    d = (char *)AlignUp((uintptr_t)d, (uintptr_t)16ULL);
    s = (char *)AlignUp((uintptr_t)s, (uintptr_t)16ULL);
    len -= std::min(bytes_before_alignment_boundary, len);
  }

  if(len >= 64)
    _mm_mfence();

  while(len >= 64)
  {
    __m128i *dst_cacheline = (__m128i *)d;
    __m128i *src_cacheline = (__m128i *)s;

    __m128i temp1 = _mm_stream_load_si128(src_cacheline + 0);
    __m128i temp2 = _mm_stream_load_si128(src_cacheline + 1);
    __m128i temp3 = _mm_stream_load_si128(src_cacheline + 2);
    __m128i temp4 = _mm_stream_load_si128(src_cacheline + 3);

    _mm_store_si128(dst_cacheline + 0, temp1);
    _mm_store_si128(dst_cacheline + 1, temp2);
    _mm_store_si128(dst_cacheline + 2, temp3);
    _mm_store_si128(dst_cacheline + 3, temp4);

    d += 64;
    s += 64;
    len -= 64;
  }
#endif
  /* memcpy() the tail. */
  if(len)
  {
    memcpy(d, s, len);
  }
}

RD_TEST(VK_Mem_Bench, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Memory mapping benchmark";

  bool bench = true;
  VkDeviceSize maxMemory = 500 * 1024 * 1024;
  uint32_t submits = 20;

  void Prepare(int argc, char **argv)
  {
    for(int i = 0; i < argc; i++)
    {
      if(!strcmp(argv[i], "--bench"))
      {
        bench = true;
      }
      if(!strcmp(argv[i], "--maxmem") && i + 1 < argc)
      {
        maxMemory = atoi(argv[i + 1]) * 1024 * 1024;
      }
      if(!strcmp(argv[i], "--submits") && i + 1 < argc)
      {
        submits = atoi(argv[i + 1]);
      }
    }

    forceComputeQueue = true;
    forceTransferQueue = true;

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             mainWindow->format,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, mainWindow->format));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(mainWindow->format, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_CLEAR));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})});

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    VkFramebuffer framebuffer = createFramebuffer(
        vkh::FramebufferCreateInfo(renderPass, {imgview}, mainWindow->scissor.extent));

    pipeCreateInfo.renderPass = renderPass;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri) + 128 * 1024,
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    const VkPhysicalDeviceMemoryProperties *props = NULL;
    vmaGetMemoryProperties(allocator, &props);

    std::vector<Alloc> allocs;

    VkDeviceSize refDataSize = 0;

    uint32_t heapTypeCount[16] = {};

    for(uint32_t m = 0; m < props->memoryTypeCount; m++)
    {
      const VkMemoryType &type = props->memoryTypes[m];
      if((type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
      {
        heapTypeCount[type.heapIndex]++;
      }
    }

    for(uint32_t m = 0; m < props->memoryTypeCount; m++)
    {
      const VkMemoryType &type = props->memoryTypes[m];
      const VkMemoryHeap &heap = props->memoryHeaps[type.heapIndex];
      if((type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
      {
        Alloc alloc;
        alloc.device = device;
        alloc.type = m;
        alloc.flags = type.propertyFlags;

        VkMemoryAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize =
            AlignUp(std::min(maxMemory, ((heap.size * 7) / 10) / heapTypeCount[type.heapIndex]),
                    (VkDeviceSize)256ULL);
        info.memoryTypeIndex = m;

        vkAllocateMemory(device, &info, NULL, &alloc.mem);
        vkCreateBuffer(
            device,
            vkh::BufferCreateInfo(info.allocationSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            NULL, &alloc.buf);
        vkBindBufferMemory(device, alloc.buf, alloc.mem, 0);

        alloc.size = info.allocationSize;

        if(bench && alloc.size > refDataSize)
          refDataSize = alloc.size;

        alloc.name = fmt::format("Mem {} ({:04}MB):", m, info.allocationSize >> 20);
        if((type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0)
          alloc.name += " CACHED";
        if((type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0)
          alloc.name += " COHERENT";
        if((type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0)
          alloc.name += " DEVICE";

        allocs.push_back(alloc);
      }
    }

    if(bench)
      refData = new byte[(uint32_t)refDataSize];

    AllocatedBuffer readback;

    if(bench)
      readback =
          AllocatedBuffer(this, vkh::BufferCreateInfo(refDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_UNKNOWN,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_CACHED_BIT}));

    using ScanFunction = std::function<std::string(Alloc &, byte *, VkDeviceSize)>;

    byte *scratch = new byte[65536];

    std::vector<ScanFunction> scanners;
    auto blockScan = [scratch](VkDeviceSize blockSize, bool streamMemcpy, Alloc &, byte *data,
                               VkDeviceSize size) {
      if(blockSize == 0)
      {
        FindDiffRange_shipping::FindDiffRange(data, refData, (size_t)size, dummyStart, dummyEnd);
        return;
      }

      for(VkDeviceSize i = 0; i < size; i += blockSize)
      {
        size_t chunkSize = (size_t)std::min(blockSize, size - i);
        if(streamMemcpy)
          stream_memcpy(scratch, data + i, chunkSize);
        else
          memcpy(scratch, data + i, chunkSize);
        FindDiffRange_shipping::FindDiffRange(scratch, refData, chunkSize, dummyStart, dummyEnd);
      }
    };

    scanners.push_back([&blockScan](Alloc &a, byte *data, VkDeviceSize size) {
      blockScan(0, false, a, data, size);
      return "direct";
    });
    scanners.push_back([&blockScan](Alloc &a, byte *data, VkDeviceSize size) {
      blockScan(128, false, a, data, size);
      return "block_128";
    });
    scanners.push_back([&blockScan](Alloc &a, byte *data, VkDeviceSize size) {
      blockScan(1024, false, a, data, size);
      return "block_1024";
    });
    scanners.push_back([&blockScan](Alloc &a, byte *data, VkDeviceSize size) {
      blockScan(65536, false, a, data, size);
      return "block_65536";
    });
    scanners.push_back([&blockScan](Alloc &a, byte *data, VkDeviceSize size) {
      blockScan(128, true, a, data, size);
      return "block_128_stream";
    });
    scanners.push_back([&blockScan](Alloc &a, byte *data, VkDeviceSize size) {
      blockScan(1024, true, a, data, size);
      return "block_1024_stream";
    });
    scanners.push_back([&blockScan](Alloc &a, byte *data, VkDeviceSize size) {
      blockScan(65536, true, a, data, size);
      return "block_65536_stream";
    });

    auto gpuReadback = [this, &readback](VkQueue q, VkCommandBuffer cmd, Alloc &a) -> byte * {
      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkBufferCopy region = {0, 0, a.size};

      vkCmdCopyBuffer(cmd, a.buf, readback.buffer, 1, &region);

      vkh::cmdPipelineBarrier(cmd, {},
                              {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                                        VK_ACCESS_HOST_READ_BIT, readback.buffer)});

      vkEndCommandBuffer(cmd);

      std::vector<VkCommandBuffer> cmds = {cmd};
      VkSubmitInfo submit = vkh::SubmitInfo(cmds);
      CHECK_VKR(vkQueueSubmit(q, 1, &submit, VK_NULL_HANDLE));
      vkQueueWaitIdle(q);

      byte *ret = readback.map();

      if((a.flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
      {
        VmaAllocationInfo info;
        vmaGetAllocationInfo(allocator, readback.alloc, &info);

        VkMappedMemoryRange range = {
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, info.deviceMemory, info.offset, info.size,
        };

        vkInvalidateMappedMemoryRanges(device, 1, &range);
      }

      return ret;
    };

    struct GPUReadbackFamily
    {
      uint32_t family;
      std::string name;
      VkQueue q;
      VkCommandPool pool;
      VkCommandBuffer cmd;
    };

    GPUReadbackFamily readbackFamily[3] = {
        {queueFamilyIndex, "default"},
        {computeQueueFamilyIndex, "compute"},
        {transferQueueFamilyIndex, "transfer"},
    };
    for(size_t i = 0; i < ARRAY_COUNT(readbackFamily); i++)
    {
      GPUReadbackFamily &f = readbackFamily[i];
      if(f.family == ~0U)
        continue;

      vkGetDeviceQueue(device, f.family, 0, &f.q);

      CHECK_VKR(vkCreateCommandPool(
          device,
          vkh::CommandPoolCreateInfo(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, f.family),
          NULL, &f.pool));

      CHECK_VKR(vkAllocateCommandBuffers(device, vkh::CommandBufferAllocateInfo(f.pool, 1), &f.cmd));

      scanners.push_back(
          [this, &readback, f, &blockScan, &gpuReadback](Alloc &a, byte *, VkDeviceSize size) {
            vkResetCommandBuffer(f.cmd, 0);

            byte *data = gpuReadback(f.q, f.cmd, a);
            blockScan(0, false, a, data, size);
            readback.unmap();

            return "gpu_" + f.name + "_direct";
          });

      scanners.push_back(
          [this, &readback, f, &blockScan, &gpuReadback](Alloc &a, byte *, VkDeviceSize size) {
            vkResetCommandBuffer(f.cmd, 0);

            byte *data = gpuReadback(f.q, f.cmd, a);
            blockScan(128, false, a, data, size);
            readback.unmap();

            return "gpu_" + f.name + "_128";
          });

      scanners.push_back(
          [this, &readback, f, &blockScan, &gpuReadback](Alloc &a, byte *, VkDeviceSize size) {
            vkResetCommandBuffer(f.cmd, 0);

            byte *data = gpuReadback(f.q, f.cmd, a);
            blockScan(128, true, a, data, size);
            readback.unmap();

            return "gpu_" + f.name + "_128_streaming";
          });

      scanners.push_back(
          [this, &readback, f, &blockScan, &gpuReadback](Alloc &a, byte *, VkDeviceSize size) {
            vkResetCommandBuffer(f.cmd, 0);

            byte *data = gpuReadback(f.q, f.cmd, a);
            blockScan(1024, false, a, data, size);
            readback.unmap();

            return "gpu_" + f.name + "_1024";
          });

      scanners.push_back(
          [this, &readback, f, &blockScan, &gpuReadback](Alloc &a, byte *, VkDeviceSize size) {
            vkResetCommandBuffer(f.cmd, 0);

            byte *data = gpuReadback(f.q, f.cmd, a);
            blockScan(1024, true, a, data, size);
            readback.unmap();

            return "gpu_" + f.name + "_1024_streaming";
          });
    }

    typedef std::chrono::high_resolution_clock Clock;
    typedef std::chrono::time_point<Clock> Time;

    uint32_t seed = 0x31F10ca8;
    for(size_t i = 0; i < refDataSize; i++)
    {
      seed = (~seed) ^ (seed >> 5);
      refData[i] = seed & 0xff;
    }

    while(Running())
    {
      for(Alloc &a : allocs)
      {
        a.map();

        if(bench)
        {
#if defined(WIN32) || defined(_WIN32)
          char OSName[] = "Windows";
#else
          char OSName[] = "Linux";
#endif
          TEST_LOG("-------- %s on %s", physProperties.deviceName, OSName);

          memcpy(a.data, refData, (size_t)a.size);
        }

        double bestSpeed = 0.0;
        std::string bestScannerName;
        for(size_t scan = 0; scan < scanners.size(); scan++)
        {
          Time prev = Clock::now();

          std::string scannerName;

          int submitsCompleted = 0;
          for(uint32_t s = 0; s < submits; s++)
          {
            VkCommandBuffer cmd = GetCommandBuffer();

            vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

            VkBufferCopy region;
            region.size = 128;
            region.srcOffset = 0;
            region.dstOffset = 128;
            vkCmdCopyBuffer(cmd, a.buf, a.buf, 1, &region);

            vkCmdBeginRenderPass(
                cmd,
                vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor,
                                         {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f)}),
                VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
            vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
            vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
            vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
            vkCmdDraw(cmd, 3, 1, 0, 0);

            vkCmdEndRenderPass(cmd);

            vkEndCommandBuffer(cmd);

            if(bench)
              scannerName = scanners[scan](a, a.data, a.size);

            Submit(99, 99, {cmd});
            submitsCompleted++;

            if(bench)
            {
              Time cur = Clock::now();
              double timeMS =
                  double(std::chrono::duration_cast<std::chrono::microseconds>(cur - prev).count()) /
                  1000.0;
              if(timeMS > 10000)
                break;
            }
          }

          if(!bench)
            break;

          Time cur = Clock::now();
          double timeMS =
              double(std::chrono::duration_cast<std::chrono::microseconds>(cur - prev).count()) /
              1000.0;

          std::string data = a.name;
          data.resize(32, ' ');
          data += "scanned by ";
          data += scannerName;
          data.resize(70, ' ');
          double speed = double((submitsCompleted * a.size) >> 20) / double(timeMS / 1000.0);
          data += fmt::format("{:8.2f} MS for {} submits = {:8.2f} MB/s", timeMS, submitsCompleted,
                              speed);
          TEST_LOG("%s", data.c_str());
          if(speed > bestSpeed * 1.02)
          {
            bestScannerName = scannerName;
            bestSpeed = speed;
          }
        }

        if(bench)
        {
          TEST_LOG("--------");
          TEST_LOG("%s's best scanner is %s", a.name.c_str(), bestScannerName.c_str());
          TEST_LOG("--------");
        }

        a.unmap();
      }
      if(bench)
        TEST_LOG("");

      {
        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        VkImage swapimg =
            StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                             VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                             VK_IMAGE_LAYOUT_GENERAL, img.image),
                 });

        blitToSwap(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL, swapimg, VK_IMAGE_LAYOUT_GENERAL);

        FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkEndCommandBuffer(cmd);

        Submit(0, 1, {cmd});
      }

      Present();
    }

    for(size_t i = 0; i < ARRAY_COUNT(readbackFamily); i++)
      vkDestroyCommandPool(device, readbackFamily[i].pool, NULL);
    for(Alloc &a : allocs)
    {
      vkDestroyBuffer(device, a.buf, NULL);
      vkFreeMemory(device, a.mem, NULL);
    }

    return 0;
  }
};

REGISTER_TEST();
