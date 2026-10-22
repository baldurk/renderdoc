/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2025-2026 Baldur Karlsson
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

#include "vk_test.h"

RD_TEST(VK_Groupshared, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Test of compute shader that uses groupshared memory.";

  std::string comp = R"EOSHADER(
#version 460 core

#define MAX_THREADS 64

layout(push_constant) uniform PushData
{
  uint test;
} push;

layout(binding = 0, std430) buffer indataBuf
{
  float indata[MAX_THREADS];
};

layout(binding = 1, std430) buffer outdataBuf
{
  vec4 outdata[MAX_THREADS];
};

shared float gsmData[MAX_THREADS];
shared int gsmIntData[MAX_THREADS];
shared int gInt;
shared float gsmSmallArray[MAX_THREADS];
shared vec3 gsmSmall2DArray[3][MAX_THREADS];

#define IsTest(x) (push.test == x)

float GetGSMValue(uint i)
{
  return gsmData[i % MAX_THREADS];
}

int GetGSMIntValue(uint i)
{
  return gsmIntData[i % MAX_THREADS];
}

layout(local_size_x = MAX_THREADS, local_size_y = 1, local_size_z = 1) in;

#define GroupMemoryBarrierWithGroupSync() memoryBarrierShared();groupMemoryBarrier();barrier();

void main()
{
  uvec3 gid = gl_LocalInvocationID;

  if(gl_LocalInvocationID.x == 0)
  {
    for(int i=0; i < MAX_THREADS; i++) gsmData[i] = 1.25f;
    for(int i=0; i < MAX_THREADS; i++) gsmIntData[i] = 125;
    gInt = 25;
  }

  GroupMemoryBarrierWithGroupSync();

  vec4 outval = vec4(0.0);
  int u = int(gid.x);

  if (IsTest(0))
  {
    // first write, should be the init value for all threads
    outval.x = GetGSMValue(gid.x);

    gsmData[gid.x] = indata[gid.x];

    // second write, should be the read value because we're reading our own value
    outval.y = GetGSMValue(gid.x);

    GroupMemoryBarrierWithGroupSync();

    // third write, should be our pairwise neighbour's value
    outval.z = GetGSMValue(gid.x ^ 1);

    // do calculation with our neighbour
    float value = (1.0f + GetGSMValue(gid.x)) * (1.0f + GetGSMValue(gid.x ^ 1));
    GroupMemoryBarrierWithGroupSync();
    gsmData[gid.x] = value;
    GroupMemoryBarrierWithGroupSync();

    // fourth write, our neighbour should be identical to our value
    outval.w = GetGSMValue(gid.x) == GetGSMValue(gid.x ^ 1) ? 9.99f : -9.99f;
  }
  else if (IsTest(1))
  {
    gsmData[gid.x] = float(gid.x);
    gsmData[gid.x] += 10.0f;
    GroupMemoryBarrierWithGroupSync();

    outval.x = GetGSMValue(gid.x);
    outval.y = GetGSMValue(gid.x + 1);

    GroupMemoryBarrierWithGroupSync();
    gsmData[gid.x] += 10.0f;
    GroupMemoryBarrierWithGroupSync();

    outval.z = GetGSMValue(gid.x + 2);

    GroupMemoryBarrierWithGroupSync();
    gsmData[gid.x] += 10.0f;
    GroupMemoryBarrierWithGroupSync();

    outval.w = GetGSMValue(gid.x + 3);
  }
  else if (IsTest(2))
  {
    // Deliberately no sync to test debugger behaviour not GPU correctness
    // Debugger should see the initial value of 1.25f for all of GSM
    gsmData[gid.x] = float(gid.x);
    outval.x = GetGSMValue(gid.x);
    outval.y = GetGSMValue(gid.x + 1);
    outval.z = GetGSMValue(gid.x + 2);
    outval.w = GetGSMValue(gid.x + 3);
  }
  else if (IsTest(3))
  {
    int value = int(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    atomicAdd(gsmIntData[u], value);
    atomicAdd(gsmIntData[u], -value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = float(GetGSMIntValue(u+0));
    outval.y = float(GetGSMIntValue(u+1));
    outval.z = float(GetGSMIntValue(u+2));
    outval.w = float(GetGSMIntValue(u+3));
  }
  else if (IsTest(4))
  {
    int value = int(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    atomicAnd(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = float(GetGSMIntValue(u+0));
    outval.y = float(GetGSMIntValue(u+1));
    outval.z = float(GetGSMIntValue(u+2));
    outval.w = float(GetGSMIntValue(u+3));
  }
  else if (IsTest(5))
  {
    int value = int(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    atomicOr(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = float(GetGSMIntValue(u+0));
    outval.y = float(GetGSMIntValue(u+1));
    outval.z = float(GetGSMIntValue(u+2));
    outval.w = float(GetGSMIntValue(u+3));
  }
  else if (IsTest(6))
  {
    int value = int(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    atomicXor(gsmIntData[u], value);
    atomicXor(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = float(GetGSMIntValue(u+0));
    outval.y = float(GetGSMIntValue(u+1));
    outval.z = float(GetGSMIntValue(u+2));
    outval.w = float(GetGSMIntValue(u+3));
  }
  else if (IsTest(7))
  {
    int value = int(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    atomicMin(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = float(GetGSMIntValue(u+0));
    outval.y = float(GetGSMIntValue(u+1));
    outval.z = float(GetGSMIntValue(u+2));
    outval.w = float(GetGSMIntValue(u+3));
  }
  else if (IsTest(8))
  {
    int value = int(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    atomicMax(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = float(GetGSMIntValue(u+0));
    outval.y = float(GetGSMIntValue(u+1));
    outval.z = float(GetGSMIntValue(u+2));
    outval.w = float(GetGSMIntValue(u+3));
  }
  else if (IsTest(9))
  {
    int value = int(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    int original = atomicExchange(gsmIntData[u], value);
    GroupMemoryBarrierWithGroupSync();
    outval.x = float(GetGSMIntValue(u+0));
    outval.y = float(GetGSMIntValue(u+1));
    outval.z = float(GetGSMIntValue(u+2));
    outval.w = float(GetGSMIntValue(u+3));
  }
  else if (IsTest(10))
  {
    int value = int(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    int original = atomicCompSwap(gsmIntData[u], value, value+1);
    GroupMemoryBarrierWithGroupSync();
    outval.x = float(GetGSMIntValue(u+0));
    outval.y = float(GetGSMIntValue(u+1));
    outval.z = float(GetGSMIntValue(u+2));
    outval.w = float(GetGSMIntValue(u+3));
  }
  else if (IsTest(11))
  {
    int value = int(indata[gid.x] * 100.0);
    gsmIntData[gid.x] = u;
    GroupMemoryBarrierWithGroupSync();
    atomicCompSwap(gsmIntData[u], value, value+1);
    GroupMemoryBarrierWithGroupSync();
    outval.x = float(GetGSMIntValue(u+0));
    outval.y = float(GetGSMIntValue(u+1));
    outval.z = float(GetGSMIntValue(u+2));
    outval.w = float(GetGSMIntValue(u+3));
  }
  else if (IsTest(12))
  {
    GroupMemoryBarrierWithGroupSync();
    outval.x = gInt;
    GroupMemoryBarrierWithGroupSync();
    atomicAdd(gInt,1);
    GroupMemoryBarrierWithGroupSync();
    outval.y = gInt;
    GroupMemoryBarrierWithGroupSync();
    atomicAdd(gInt,1);
    GroupMemoryBarrierWithGroupSync();
    outval.z = gInt;
    GroupMemoryBarrierWithGroupSync();
    atomicAdd(gInt,1);
    GroupMemoryBarrierWithGroupSync();
    outval.w = gInt;
  }
  else if (IsTest(13))
  {
    // Out of bounds GSM access should not crash
    uint idx = gid.x;
    uint oneu = (idx+1)/(idx+1);
    uint zerou = oneu - 1;
    uint idxOOB = idx * 2 + MAX_THREADS * oneu;

    gsmSmallArray[idx] = float(gid.x);
    gsmSmall2DArray[zerou][idx].x = float(gid.x);
    gsmSmall2DArray[zerou][idx].y = float(gid.x) + 1.0;
    gsmSmall2DArray[oneu][idx].x = float(gid.x);
    gsmSmall2DArray[oneu][idx].y = float(gid.x) + 1.0;
    gsmSmall2DArray[oneu+oneu][idx].x = float(gid.x);
    gsmSmall2DArray[oneu+oneu][idx].y = float(gid.x) + 1.0;

    gsmSmallArray[idxOOB] += float(zerou);
    gsmSmall2DArray[oneu][idxOOB].x += float(zerou);
    gsmSmall2DArray[oneu][idxOOB].y += float(zerou);

    outval.x = gsmSmallArray[idx + zerou];
    outval.y = gsmSmallArray[idx + zerou];
    outval.z = gsmSmall2DArray[oneu][idx + zerou].x;
    outval.w = gsmSmall2DArray[oneu][idx + zerou].y;
  }

  outdata[gid.x] = outval;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setLayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
    }));
    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {setLayout}, {vkh::PushConstantRange(VK_SHADER_STAGE_ALL, 0, 4)}));

    VkPipeline pipe = createComputePipeline(vkh::ComputePipelineCreateInfo(
        layout, CompileShaderModule(comp, ShaderLang::glsl, ShaderStage::comp)));

    VkDescriptorSet descSet = allocateDescriptorSet(setLayout);

    float values[64];
    for(int i = 0; i < 64; i++)
      values[i] = RANDF(1.0f, 100.0f);
    AllocatedBuffer inBuf(this,
                          vkh::BufferCreateInfo(sizeof(values), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    inBuf.upload(values);

    AllocatedBuffer outBuf(
        this,
        vkh::BufferCreateInfo(sizeof(Vec4f) * 64, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(inBuf.buffer)}),
                    vkh::WriteDescriptorSet(descSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(outBuf.buffer)}),
                });

    int numCompTests = 0;
    size_t pos = 0;
    while(pos != std::string::npos)
    {
      pos = comp.find("IsTest(", pos);
      if(pos == std::string::npos)
        break;
      pos += sizeof("IsTest(") - 1;
      numCompTests = std::max(numCompTests, atoi(comp.c_str() + pos) + 1);
    }

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg = StartUsingBackbuffer(cmd);

      vkh::cmdClearImage(cmd, swapimg, vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f));

      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, {descSet}, {});
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);

      pushMarker(cmd, "Compute Tests");
      for(int i = 0; i < numCompTests; ++i)
      {
        vkh::cmdPipelineBarrier(
            cmd, {},
            {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                      outBuf.buffer)});

        vkCmdFillBuffer(cmd, outBuf.buffer, 0, sizeof(Vec4f) * 64, 0);
        vkh::cmdPipelineBarrier(
            cmd, {},
            {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                      outBuf.buffer)});

        vkh::cmdPushConstants(cmd, layout, i);
        vkCmdDispatch(cmd, 1, 1, 1);
      }
      popMarker(cmd);

      FinishUsingBackbuffer(cmd);

      vkEndCommandBuffer(cmd);

      SubmitAndPresent({cmd});
    }

    return 0;
  }
};

REGISTER_TEST();
