/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#include "hlsl_cbuffers.h"

RWStructuredBuffer<InstanceDesc> instanceDescs : register(u0);
StructuredBuffer<BlasAddressPair> oldNewAddressesPair : register(t0);

bool InRange(BlasAddressRange addressRange, GPUAddress address)
{
  if(lessEqual(addressRange.start, address) && lessThan(address, addressRange.end))
  {
    return true;
  }

  return false;
}

// Each SV_GroupId corresponds to each of the BLAS (instance) in TLAS
[numthreads(1, 1, 1)] void RENDERDOC_PatchAccStructAddressCS(uint3 dispatchGroup
                                                             : SV_GroupId) {
  GPUAddress instanceBlasAddress = instanceDescs[dispatchGroup.x].blasAddress;

  for(uint i = 0; i < addressCount; i++)
  {
    if(InRange(oldNewAddressesPair[i].oldAddress, instanceBlasAddress))
    {
      GPUAddress offset = sub(instanceBlasAddress, oldNewAddressesPair[i].oldAddress.start);
      instanceDescs[dispatchGroup.x].blasAddress =
          add(oldNewAddressesPair[i].newAddress.start, offset);
      return;
    }
  }

  // This  might cause device hang but at least we won't access incorrect addresses
  instanceDescs[dispatchGroup.x].blasAddress = 0;
}

StructuredBuffer<StateObjectLookup> stateObjects : register(t1);
StructuredBuffer<ShaderRecordData> records : register(t2);
StructuredBuffer<LocalRootSigData> rootsigs : register(t3);

struct WrappedRecord
{
  uint2 id;    // ResourceId
  uint index;
};

StructuredBuffer<BlasAddressPair> patchAddressesPair : register(t4);

ByteAddressBuffer patchSource : register(t0);
RWByteAddressBuffer patchDest : register(u0);

struct DescriptorHeapData
{
  GPUAddress wrapped_base;
  GPUAddress wrapped_end;

  GPUAddress unwrapped_base;

  uint unwrapped_stride;
};

uint CopyData(ByteAddressBuffer src, RWByteAddressBuffer dst, uint byteOffset, uint dataOffset,
              uint dataEnd)
{
  uint4 data = 0;
  uint readBytes = 0;

  // copy larger chunks if possible
  if(dataOffset + 16 <= dataEnd)
  {
    data = src.Load4(byteOffset + dataOffset);
    dst.Store4(byteOffset + dataOffset, data);
    readBytes = 16;
  }
  else if(dataOffset + 12 <= dataEnd)
  {
    data = src.Load3(byteOffset + dataOffset).xyzx;
    dst.Store3(byteOffset + dataOffset, data.xyz);
    readBytes = 12;
  }
  else if(dataOffset + 8 <= dataEnd)
  {
    data = src.Load2(byteOffset + dataOffset).xyxx;
    dst.Store2(byteOffset + dataOffset, data.xy);
    readBytes = 8;
  }
  else if(dataOffset + 4 <= dataEnd)
  {
    data = src.Load(byteOffset + dataOffset).xxxx;
    dst.Store(byteOffset + dataOffset, data.x);
    readBytes = 4;
  }
  else
  {
    readBytes = dataEnd;
  }

  // don't copy anything here, everything should be uint32 aligned
  return dataOffset + readBytes;
}

void PatchTable(uint byteOffset)
{
  // load our wrapped record from the start of the table
  WrappedRecord wrappedRecord;
  wrappedRecord.id = patchSource.Load2(byteOffset);
  wrappedRecord.index = patchSource.Load(byteOffset + 8);

  // find the state object it came from
  int i = 0;
  StateObjectLookup objectLookup;
  [allow_uav_condition] do
  {
    objectLookup = stateObjects[i];

    if(objectLookup.id.x == wrappedRecord.id.x && objectLookup.id.y == wrappedRecord.id.y)
      break;

    i++;

    // terminate when the lookup is empty, we're out of state objects
  }
  while(objectLookup.id.x != 0 || objectLookup.id.y != 0)
    ;

  // if didn't find a match, set a NULL shader identifier. This will fail if it's raygen but others
  // will in theory not crash.
  if(objectLookup.id.x == 0 && objectLookup.id.y == 0)
  {
    patchDest.Store4(byteOffset, uint4(0, 0, 0, 0));
    patchDest.Store4(byteOffset + 16, uint4(0, 0, 0, 0));
    return;
  }

  // the exports from this state object are contiguous starting from the given index, look up this
  // identifier's export
  ShaderRecordData recordData = records[objectLookup.offset + wrappedRecord.index];

  // store the unwrapped shader identifier
  patchDest.Store4(byteOffset, recordData.identifier[0]);
  patchDest.Store4(byteOffset + 16, recordData.identifier[1]);

  // size of a shader record, which we've just copied/patched above
  uint firstUncopiedByte = 32;

  uint rootSigIndex = (recordData.rootSigIndex & 0xffff);

  if(rootSigIndex != 0xffff)
  {
    LocalRootSigData sig = rootsigs[rootSigIndex];

    DescriptorHeapData heaps[2];

    heaps[0].wrapped_base = wrapped_sampHeapBase;
    heaps[1].wrapped_base = wrapped_srvHeapBase;

    heaps[0].wrapped_end = add(wrapped_sampHeapBase, GPUAddress(wrapped_sampHeapSize, 0));
    heaps[1].wrapped_end = add(wrapped_srvHeapBase, GPUAddress(wrapped_srvHeapSize, 0));

    heaps[0].unwrapped_stride = unwrapped_heapStrides & 0xffff;
    heaps[1].unwrapped_stride = unwrapped_heapStrides >> 16;

    heaps[0].unwrapped_base = unwrapped_sampHeapBase;
    heaps[1].unwrapped_base = unwrapped_srvHeapBase;

    [allow_uav_condition] for(uint i = 0; i < sig.numParams; i++)
    {
      uint paramOffset = sig.paramOffsets[i] & 0xffff;
      bool isHandle = (sig.paramOffsets[i] & 0xffff0000) == 0;

      // copy any gap from where we last processed
      for(uint b = firstUncopiedByte; b < paramOffset;)
      {
        b = CopyData(patchSource, patchDest, byteOffset, b, paramOffset);
      }
      firstUncopiedByte = paramOffset + 8;

      if(isHandle)
      {
        GPUAddress wrappedHandlePtr = patchSource.Load2(byteOffset + paramOffset);

        bool patched = false;
        [allow_uav_condition] for(int h = 0; h < 2; h++)
        {
          if(lessEqual(heaps[h].wrapped_base, wrappedHandlePtr) &&
             lessThan(wrappedHandlePtr, heaps[h].wrapped_end))
          {
            // assume the byte offsets will all fit into the LSB 32-bits
            uint index = sub(wrappedHandlePtr, heaps[h].wrapped_base).x / WRAPPED_DESCRIPTOR_STRIDE;

            GPUAddress handleOffset = GPUAddress(index * heaps[h].unwrapped_stride, 0);
            GPUAddress unwrapped = add(heaps[h].unwrapped_base, handleOffset);
            patchDest.Store2(byteOffset + paramOffset, unwrapped);
            patched = true;
            break;
          }
        }

        if(!patched)
        {
          // won't work but is our best effort
          patchDest.Store2(byteOffset + paramOffset, GPUAddress(0, 0));
        }
      }
      else
      {
        // during capture addresses don't have to be patched, only patch them on replay
        if(numPatchingAddrs > 0)
        {
          GPUAddress origAddress = patchSource.Load2(byteOffset + paramOffset);

          [allow_uav_condition] for(uint i = 0; i < numPatchingAddrs; i++)
          {
            if(InRange(patchAddressesPair[i].oldAddress, origAddress))
            {
              GPUAddress offset = sub(origAddress, patchAddressesPair[i].oldAddress.start);
              patchDest.Store2(byteOffset + paramOffset,
                               add(patchAddressesPair[i].newAddress.start, offset));
              break;
            }
          }
        }
        else
        {
          GPUAddress origAddress = patchSource.Load2(byteOffset + paramOffset);
          patchDest.Store2(byteOffset + paramOffset, origAddress);
        }
      }
    }

    // copy any remaining trailing bytes
    for(uint b = firstUncopiedByte; b < shaderrecord_stride;)
    {
      b = CopyData(patchSource, patchDest, byteOffset, b, shaderrecord_stride);
    }
  }
  else
  {
    // no root sig data to patch, just copy the whole stride
    for(uint b = firstUncopiedByte; b < shaderrecord_stride;)
    {
      b = CopyData(patchSource, patchDest, byteOffset, b, shaderrecord_stride);
    }
  }
}

// Each SV_GroupId corresponds to one shader record to patch
[numthreads(RECORD_PATCH_THREADS, 1, 1)] void RENDERDOC_PatchRayDispatchCS(uint3 dispatchThread
                                                                           : SV_DispatchThreadID) {
  if(dispatchThread.x < shaderrecord_count)
    PatchTable(shaderrecord_stride * dispatchThread.x);
}

// define these structs in hlsl for simplicity

struct D3D12_GPU_VIRTUAL_ADDRESS_RANGE
{
  GPUAddress StartAddress;
  uint2 SizeInBytes;    // we ignore the high order bits in .y
};

struct D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE
{
  GPUAddress StartAddress;
  uint2 SizeInBytes;      // we ignore the high order bits in .y
  uint2 StrideInBytes;    // we ignore the high order bits in .y
};

struct D3D12_DISPATCH_RAYS_DESC
{
  D3D12_GPU_VIRTUAL_ADDRESS_RANGE RayGenerationShaderRecord;
  D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE MissShaderTable;
  D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE HitGroupTable;
  D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE CallableShaderTable;
  uint Width;
  uint Height;
  uint Depth;
};

struct D3D12_DISPATCH_RAYS_DESC_RAW
{
  uint4 a[6];
  uint b;
};

ByteAddressBuffer applicationExecuteArguments : register(t0);
ByteAddressBuffer applicationCountBuffer : register(t1);
RWByteAddressBuffer patchedExecuteArguments : register(u0);
RWStructuredBuffer<PatchingExecute> internalExecuteArguments : register(u1);
RWByteAddressBuffer internalExecuteCount : register(u2);

GPUAddress AlignRecordAddress(GPUAddress x)
{
  GPUAddress ret = add(x, GPUAddress(0xff, 0));
  ret.x &= ~0xff;
  return ret;
}

[numthreads(1, 1, 1)] void RENDERDOC_PrepareRayIndirectExecuteCS() {
  uint numCommands = maxCommandCount;
  if(numCommands & 0x80000000U)
  {
    numCommands &= 0x7fffffffU;
  }
  else
  {
    numCommands = min(numCommands, applicationCountBuffer.Load(0));
  }

  GPUAddress outputBufferLocation = scratchBuffer;
  uint dispatchIndex = 0;

  PatchingExecute execute = (PatchingExecute)0xccddeeff;

  // loop over all commands the application ran (probably just one)
  for(uint i = 0; i < numCommands; i++)
  {
    uint commandOffset = i * commandSigStride;

    // load the indirect arguments in batch as possible
    D3D12_DISPATCH_RAYS_DESC_RAW raw = {
        applicationExecuteArguments.Load4(commandSigDispatchOffset + commandOffset + 0 * 16),
        applicationExecuteArguments.Load4(commandSigDispatchOffset + commandOffset + 1 * 16),
        applicationExecuteArguments.Load4(commandSigDispatchOffset + commandOffset + 2 * 16),
        applicationExecuteArguments.Load4(commandSigDispatchOffset + commandOffset + 3 * 16),
        applicationExecuteArguments.Load4(commandSigDispatchOffset + commandOffset + 4 * 16),
        applicationExecuteArguments.Load4(commandSigDispatchOffset + commandOffset + 5 * 16),
        applicationExecuteArguments.Load(commandSigDispatchOffset + commandOffset + 6 * 16),
    };

    D3D12_DISPATCH_RAYS_DESC appCommand = (D3D12_DISPATCH_RAYS_DESC)raw;

    // align the address before each record we're going to patch into
    outputBufferLocation = AlignRecordAddress(outputBufferLocation);

    // raygen - set up an execute to patch the record
    {
      execute.shaderrecord_stride = appCommand.RayGenerationShaderRecord.SizeInBytes.x;
      execute.shaderrecord_count = 1;
      execute.sourceData = appCommand.RayGenerationShaderRecord.StartAddress;
      execute.destData = outputBufferLocation;
      execute.dispatchDim = uint3(1, 1, 1);
      internalExecuteArguments[dispatchIndex++] = execute;

      // update the dispatch desc to point to the new location
      appCommand.RayGenerationShaderRecord.StartAddress = outputBufferLocation;

      outputBufferLocation =
          add(outputBufferLocation, appCommand.RayGenerationShaderRecord.SizeInBytes);
    }

    outputBufferLocation = AlignRecordAddress(outputBufferLocation);

    if(appCommand.MissShaderTable.SizeInBytes.x > 0)
    {
      execute.shaderrecord_stride = appCommand.MissShaderTable.StrideInBytes.x;
      execute.shaderrecord_count = appCommand.MissShaderTable.SizeInBytes.x /
                                   max(1, appCommand.MissShaderTable.StrideInBytes.x);
      execute.sourceData = appCommand.MissShaderTable.StartAddress;
      execute.destData = outputBufferLocation;
      execute.dispatchDim = uint3(
          (execute.shaderrecord_count + (RECORD_PATCH_THREADS - 1)) / RECORD_PATCH_THREADS, 1, 1);
      internalExecuteArguments[dispatchIndex++] = execute;

      appCommand.MissShaderTable.StartAddress = outputBufferLocation;

      outputBufferLocation = add(outputBufferLocation, appCommand.MissShaderTable.SizeInBytes);
    }

    outputBufferLocation = AlignRecordAddress(outputBufferLocation);

    if(appCommand.HitGroupTable.SizeInBytes.x > 0)
    {
      execute.shaderrecord_stride = appCommand.HitGroupTable.StrideInBytes.x;
      execute.shaderrecord_count =
          appCommand.HitGroupTable.SizeInBytes.x / max(1, appCommand.HitGroupTable.StrideInBytes.x);
      execute.sourceData = appCommand.HitGroupTable.StartAddress;
      execute.destData = outputBufferLocation;
      execute.dispatchDim = uint3(
          (execute.shaderrecord_count + (RECORD_PATCH_THREADS - 1)) / RECORD_PATCH_THREADS, 1, 1);
      internalExecuteArguments[dispatchIndex++] = execute;

      appCommand.HitGroupTable.StartAddress = outputBufferLocation;

      outputBufferLocation = add(outputBufferLocation, appCommand.HitGroupTable.SizeInBytes);
    }

    outputBufferLocation = AlignRecordAddress(outputBufferLocation);

    if(appCommand.CallableShaderTable.SizeInBytes.x > 0)
    {
      execute.shaderrecord_stride = appCommand.CallableShaderTable.StrideInBytes.x;
      execute.shaderrecord_count = appCommand.CallableShaderTable.SizeInBytes.x /
                                   max(1, appCommand.CallableShaderTable.StrideInBytes.x);
      execute.sourceData = appCommand.CallableShaderTable.StartAddress;
      execute.destData = outputBufferLocation;
      execute.dispatchDim = uint3(
          (execute.shaderrecord_count + (RECORD_PATCH_THREADS - 1)) / RECORD_PATCH_THREADS, 1, 1);
      internalExecuteArguments[dispatchIndex++] = execute;

      appCommand.CallableShaderTable.StartAddress = outputBufferLocation;

      outputBufferLocation = add(outputBufferLocation, appCommand.CallableShaderTable.SizeInBytes);
    }

    // copy all data apart from the indirect draw (if anything) to the patched argument buffer
    // no root sig data to patch, just copy the whole stride
    for(uint b = 0; b < commandSigDispatchOffset;)
    {
      b = CopyData(applicationExecuteArguments, patchedExecuteArguments, commandOffset, b,
                   commandSigDispatchOffset);
    }

    // store the indirect command (with updated addresses) to the patched buffer
    raw = (D3D12_DISPATCH_RAYS_DESC_RAW)appCommand;

    patchedExecuteArguments.Store4(commandSigDispatchOffset + commandOffset + 0 * 16, raw.a[0]);
    patchedExecuteArguments.Store4(commandSigDispatchOffset + commandOffset + 1 * 16, raw.a[1]);
    patchedExecuteArguments.Store4(commandSigDispatchOffset + commandOffset + 2 * 16, raw.a[2]);
    patchedExecuteArguments.Store4(commandSigDispatchOffset + commandOffset + 3 * 16, raw.a[3]);
    patchedExecuteArguments.Store4(commandSigDispatchOffset + commandOffset + 4 * 16, raw.a[4]);
    patchedExecuteArguments.Store4(commandSigDispatchOffset + commandOffset + 5 * 16, raw.a[5]);
    patchedExecuteArguments.Store(commandSigDispatchOffset + commandOffset + 6 * 16, raw.b);
  }

  // store the number of patching indirect dispatches we'll do, up to 4 per the application's number
  internalExecuteCount.Store(0, dispatchIndex);
}
