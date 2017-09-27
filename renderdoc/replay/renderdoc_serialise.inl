/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

// these macros do compile time asserts on the size of the structure, to
// help prevent the structure changing without these functions being updated.
// This isn't perfect as a new variable could be added in padding space, or
// one removed and leaves padding. Most variables are 4 bytes in size though
// so it should be fairly reliable and it's better than nothing!
// Since structures contain pointers and vary in size, we do this only on
// x64 since that's the commonly built target.

#ifndef ENABLED
#define ENABLED(...) 0
#endif

#if ENABLED(RDOC_WIN32) && ENABLED(RDOC_X64) && ENABLED(RDOC_DEVEL)
template <typename T, size_t actual, size_t expected>
class oversized
{
  int check[int(actual) - int(expected) + 1];
};
template <typename T, size_t actual, size_t expected>
class undersized
{
  int check[int(expected) - int(actual) + 1];
};

#define SIZE_CHECK(expected)                        \
  undersized<decltype(el), sizeof(el), expected>(); \
  oversized<decltype(el), sizeof(el), expected>();
#else
#define SIZE_CHECK(expected)
#endif

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, PathEntry &el)
{
  SERIALISE_MEMBER(filename);
  SERIALISE_MEMBER(flags);
  SERIALISE_MEMBER(lastmod);
  SERIALISE_MEMBER(size);

  SIZE_CHECK(32);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, EnvironmentModification &el)
{
  SERIALISE_MEMBER(mod);
  SERIALISE_MEMBER(sep);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(value);

  SIZE_CHECK(40);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, CaptureOptions &el)
{
  SERIALISE_MEMBER(AllowVSync);
  SERIALISE_MEMBER(AllowFullscreen);
  SERIALISE_MEMBER(APIValidation);
  SERIALISE_MEMBER(CaptureCallstacks);
  SERIALISE_MEMBER(CaptureCallstacksOnlyDraws);
  SERIALISE_MEMBER(DelayForDebugger);
  SERIALISE_MEMBER(VerifyMapWrites);
  SERIALISE_MEMBER(HookIntoChildren);
  SERIALISE_MEMBER(RefAllResources);
  SERIALISE_MEMBER(SaveAllInitials);
  SERIALISE_MEMBER(CaptureAllCmdLists);
  SERIALISE_MEMBER(DebugOutputMute);

  SIZE_CHECK(20);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ResourceFormat &el)
{
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(compCount);
  SERIALISE_MEMBER(compByteWidth);
  SERIALISE_MEMBER(compType);
  SERIALISE_MEMBER(bgraOrder);
  SERIALISE_MEMBER(srgbCorrected);

  SIZE_CHECK(6);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, BindpointMap &el)
{
  SERIALISE_MEMBER(bindset);
  SERIALISE_MEMBER(bind);
  SERIALISE_MEMBER(used);
  SERIALISE_MEMBER(arraySize);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderBindpointMapping &el)
{
  SERIALISE_MEMBER(InputAttributes);
  SERIALISE_MEMBER(ConstantBlocks);
  SERIALISE_MEMBER(Samplers);
  SERIALISE_MEMBER(ReadOnlyResources);
  SERIALISE_MEMBER(ReadWriteResources);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, SigParameter &el)
{
  SERIALISE_MEMBER(varName);
  SERIALISE_MEMBER(semanticName);
  SERIALISE_MEMBER(semanticIndex);
  SERIALISE_MEMBER(semanticIdxName);
  SERIALISE_MEMBER(needSemanticIndex);
  SERIALISE_MEMBER(regIndex);
  SERIALISE_MEMBER(systemValue);
  SERIALISE_MEMBER(compType);
  SERIALISE_MEMBER(regChannelMask);
  SERIALISE_MEMBER(channelUsedMask);
  SERIALISE_MEMBER(compCount);
  SERIALISE_MEMBER(stream);
  SERIALISE_MEMBER(arrayIndex);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderVariableType &el)
{
  SERIALISE_MEMBER(descriptor.name);
  SERIALISE_MEMBER(descriptor.type);
  SERIALISE_MEMBER(descriptor.rows);
  SERIALISE_MEMBER(descriptor.cols);
  SERIALISE_MEMBER(descriptor.elements);
  SERIALISE_MEMBER(descriptor.rowMajorStorage);
  SERIALISE_MEMBER(descriptor.arrayStride);
  SERIALISE_MEMBER(members);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderConstant &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(reg.vec);
  SERIALISE_MEMBER(reg.comp);
  SERIALISE_MEMBER(defaultValue);
  SERIALISE_MEMBER(type);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ConstantBlock &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(variables);
  SERIALISE_MEMBER(bufferBacked);
  SERIALISE_MEMBER(bindPoint);
  SERIALISE_MEMBER(byteSize);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderSampler &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(bindPoint);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderResource &el)
{
  SERIALISE_MEMBER(IsTexture);
  SERIALISE_MEMBER(IsReadOnly);
  SERIALISE_MEMBER(resType);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(variableType);
  SERIALISE_MEMBER(bindPoint);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderCompileFlags &el)
{
  SERIALISE_MEMBER(flags);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderDebugChunk &el)
{
  SERIALISE_MEMBER(compileFlags);
  SERIALISE_MEMBER(files);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderReflection &el)
{
  SERIALISE_MEMBER(ID);
  SERIALISE_MEMBER(EntryPoint);

  SERIALISE_MEMBER(DebugInfo);

  SERIALISE_MEMBER(DispatchThreadsDimension);

  SERIALISE_MEMBER(RawBytes);

  SERIALISE_MEMBER(InputSig);
  SERIALISE_MEMBER(OutputSig);

  SERIALISE_MEMBER(ConstantBlocks);

  SERIALISE_MEMBER(Samplers);

  SERIALISE_MEMBER(ReadOnlyResources);
  SERIALISE_MEMBER(ReadWriteResources);

  SERIALISE_MEMBER(Interfaces);

  SIZE_CHECK(200);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderVariable &el)
{
  SERIALISE_MEMBER(rows);
  SERIALISE_MEMBER(columns);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(type);

  SERIALISE_MEMBER(value.dv);

  SERIALISE_MEMBER(isStruct);

  SERIALISE_MEMBER(members);

  SIZE_CHECK(184);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderDebugState &el)
{
  SERIALISE_MEMBER(registers);
  SERIALISE_MEMBER(outputs);
  SERIALISE_MEMBER(indexableTemps);
  SERIALISE_MEMBER(nextInstruction);
  SERIALISE_MEMBER(flags);

  SIZE_CHECK(56);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderDebugTrace &el)
{
  SERIALISE_MEMBER(inputs);
  SERIALISE_MEMBER(cbuffers);
  SERIALISE_MEMBER(states);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, TextureFilter &el)
{
  SERIALISE_MEMBER(minify);
  SERIALISE_MEMBER(magnify);
  SERIALISE_MEMBER(mip);
  SERIALISE_MEMBER(func);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, TextureDescription &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(customName);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(dimension);
  SERIALISE_MEMBER(resType);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(depth);
  SERIALISE_MEMBER(ID);
  SERIALISE_MEMBER(cubemap);
  SERIALISE_MEMBER(mips);
  SERIALISE_MEMBER(arraysize);
  SERIALISE_MEMBER(creationFlags);
  SERIALISE_MEMBER(msQual);
  SERIALISE_MEMBER(msSamp);
  SERIALISE_MEMBER(byteSize);

  SIZE_CHECK(88);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, BufferDescription &el)
{
  SERIALISE_MEMBER(ID);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(customName);
  SERIALISE_MEMBER(creationFlags);
  SERIALISE_MEMBER(length);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, APIProperties &el)
{
  SERIALISE_MEMBER(pipelineType);
  SERIALISE_MEMBER(localRenderer);
  SERIALISE_MEMBER(degraded);

  SIZE_CHECK(12);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DebugMessage &el)
{
  SERIALISE_MEMBER(eventID);
  SERIALISE_MEMBER(category);
  SERIALISE_MEMBER(severity);
  SERIALISE_MEMBER(source);
  SERIALISE_MEMBER(messageID);
  SERIALISE_MEMBER(description);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, APIEvent &el)
{
  SERIALISE_MEMBER(eventID);
  SERIALISE_MEMBER(callstack);
  SERIALISE_MEMBER(eventDesc);
  SERIALISE_MEMBER(fileOffset);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DrawcallDescription &el)
{
  SERIALISE_MEMBER(eventID);
  SERIALISE_MEMBER(drawcallID);

  SERIALISE_MEMBER(name);

  SERIALISE_MEMBER(flags);

  SERIALISE_MEMBER(markerColor);

  SERIALISE_MEMBER(numIndices);
  SERIALISE_MEMBER(numInstances);
  SERIALISE_MEMBER(baseVertex);
  SERIALISE_MEMBER(indexOffset);
  SERIALISE_MEMBER(vertexOffset);
  SERIALISE_MEMBER(instanceOffset);

  SERIALISE_MEMBER(dispatchDimension);
  SERIALISE_MEMBER(dispatchThreadsDimension);

  SERIALISE_MEMBER(indexByteWidth);
  SERIALISE_MEMBER(topology);

  SERIALISE_MEMBER(copySource);
  SERIALISE_MEMBER(copyDestination);

  SERIALISE_MEMBER(parent);
  SERIALISE_MEMBER(previous);
  SERIALISE_MEMBER(next);

  SERIALISE_MEMBER(outputs);
  SERIALISE_MEMBER(depthOut);

  SERIALISE_MEMBER(events);
  SERIALISE_MEMBER(children);

  SIZE_CHECK(248);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ConstantBindStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);
  SERIALISE_MEMBER(bindslots);
  SERIALISE_MEMBER(sizes);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, SamplerBindStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);
  SERIALISE_MEMBER(bindslots);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ResourceBindStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);
  SERIALISE_MEMBER(types);
  SERIALISE_MEMBER(bindslots);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ResourceUpdateStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(clients);
  SERIALISE_MEMBER(servers);
  SERIALISE_MEMBER(types);
  SERIALISE_MEMBER(sizes);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DrawcallStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(instanced);
  SERIALISE_MEMBER(indirect);
  SERIALISE_MEMBER(counts);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DispatchStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(indirect);

  SIZE_CHECK(8);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, IndexBindStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);

  SIZE_CHECK(12);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VertexBindStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);
  SERIALISE_MEMBER(bindslots);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, LayoutBindStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);

  SIZE_CHECK(12);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderChangeStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);
  SERIALISE_MEMBER(redundants);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, BlendStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);
  SERIALISE_MEMBER(redundants);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DepthStencilStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);
  SERIALISE_MEMBER(redundants);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, RasterizationStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);
  SERIALISE_MEMBER(redundants);
  SERIALISE_MEMBER(viewports);
  SERIALISE_MEMBER(rects);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, OutputTargetStats &el)
{
  SERIALISE_MEMBER(calls);
  SERIALISE_MEMBER(sets);
  SERIALISE_MEMBER(nulls);
  SERIALISE_MEMBER(bindslots);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, FrameStatistics &el)
{
  SERIALISE_MEMBER(recorded);
  SERIALISE_MEMBER(constants);
  SERIALISE_MEMBER(samplers);
  SERIALISE_MEMBER(resources);
  SERIALISE_MEMBER(updates);
  SERIALISE_MEMBER(draws);
  SERIALISE_MEMBER(dispatches);
  SERIALISE_MEMBER(indices);
  SERIALISE_MEMBER(vertices);
  SERIALISE_MEMBER(layouts);
  SERIALISE_MEMBER(shaders);
  SERIALISE_MEMBER(blends);
  SERIALISE_MEMBER(depths);
  SERIALISE_MEMBER(rasters);
  SERIALISE_MEMBER(outputs);

  SIZE_CHECK(1136);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, FrameDescription &el)
{
  SERIALISE_MEMBER(frameNumber);
  SERIALISE_MEMBER(fileOffset);
  SERIALISE_MEMBER(uncompressedFileSize);
  SERIALISE_MEMBER(compressedFileSize);
  SERIALISE_MEMBER(persistentSize);
  SERIALISE_MEMBER(initDataSize);
  SERIALISE_MEMBER(captureTime);
  SERIALISE_MEMBER(stats);
  SERIALISE_MEMBER(debugMessages);

  SIZE_CHECK(1208);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, FrameRecord &el)
{
  SERIALISE_MEMBER(frameInfo);
  SERIALISE_MEMBER(drawcallList);

  SIZE_CHECK(1224);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MeshFormat &el)
{
  SERIALISE_MEMBER(idxbuf);
  SERIALISE_MEMBER(idxoffs);
  SERIALISE_MEMBER(idxByteWidth);
  SERIALISE_MEMBER(baseVertex);
  SERIALISE_MEMBER(buf);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(stride);
  SERIALISE_MEMBER(fmt);
  SERIALISE_MEMBER(meshColor);
  SERIALISE_MEMBER(showAlpha);
  SERIALISE_MEMBER(topo);
  SERIALISE_MEMBER(numVerts);
  SERIALISE_MEMBER(unproject);
  SERIALISE_MEMBER(nearPlane);
  SERIALISE_MEMBER(farPlane);

  SIZE_CHECK(88);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, FloatVector &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(z);
  SERIALISE_MEMBER(w);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, Uuid &el)
{
  SERIALISE_MEMBER(bytes);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, CounterDescription &el)
{
  SERIALISE_MEMBER(counterID);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(description);
  SERIALISE_MEMBER(resultType);
  SERIALISE_MEMBER(resultByteWidth);
  SERIALISE_MEMBER(unit);
  SERIALISE_MEMBER(category);
  SERIALISE_MEMBER(uuid);

  SIZE_CHECK(88);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, PixelValue &el)
{
  SERIALISE_MEMBER(value_u);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, PixelModification &el)
{
  SERIALISE_MEMBER(eventID);

  SERIALISE_MEMBER(directShaderWrite);
  SERIALISE_MEMBER(unboundPS);

  SERIALISE_MEMBER(fragIndex);
  SERIALISE_MEMBER(primitiveID);

  SERIALISE_MEMBER(preMod.col.value_u);
  SERIALISE_MEMBER(preMod.depth);
  SERIALISE_MEMBER(preMod.stencil);
  SERIALISE_MEMBER(shaderOut.col.value_u);
  SERIALISE_MEMBER(shaderOut.depth);
  SERIALISE_MEMBER(shaderOut.stencil);
  SERIALISE_MEMBER(postMod.col.value_u);
  SERIALISE_MEMBER(postMod.depth);
  SERIALISE_MEMBER(postMod.stencil);

  SERIALISE_MEMBER(sampleMasked);
  SERIALISE_MEMBER(backfaceCulled);
  SERIALISE_MEMBER(depthClipped);
  SERIALISE_MEMBER(viewClipped);
  SERIALISE_MEMBER(scissorClipped);
  SERIALISE_MEMBER(shaderDiscarded);
  SERIALISE_MEMBER(depthTestFailed);
  SERIALISE_MEMBER(stencilTestFailed);

  SIZE_CHECK(96);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, EventUsage &el)
{
  SERIALISE_MEMBER(eventID);
  SERIALISE_MEMBER(usage);
  SERIALISE_MEMBER(view);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, CounterResult &el)
{
  SERIALISE_MEMBER(eventID);
  SERIALISE_MEMBER(counterID);
  SERIALISE_MEMBER(value);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, CounterValue &el)
{
  SERIALISE_MEMBER(u64);

  SIZE_CHECK(8);
}

#pragma region D3D11 pipeline state

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Layout &el)
{
  SERIALISE_MEMBER(SemanticName);
  SERIALISE_MEMBER(SemanticIndex);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(InputSlot);
  SERIALISE_MEMBER(ByteOffset);
  SERIALISE_MEMBER(PerInstance);
  SERIALISE_MEMBER(InstanceDataStepRate);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::VB &el)
{
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(Stride);
  SERIALISE_MEMBER(Offset);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::IB &el)
{
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(Offset);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::IA &el)
{
  SERIALISE_MEMBER(layouts);
  SERIALISE_MEMBER(layout);

  SERIALISE_MEMBER(customName);
  SERIALISE_MEMBER(name);

  SERIALISE_MEMBER(vbuffers);
  SERIALISE_MEMBER(ibuffer);

  if(ser.IsReading())
    el.Bytecode = NULL;

  SIZE_CHECK(88);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::View &el)
{
  SERIALISE_MEMBER(Object);
  SERIALISE_MEMBER(Resource);
  SERIALISE_MEMBER(Type);
  SERIALISE_MEMBER(Format);

  SERIALISE_MEMBER(Structured);
  SERIALISE_MEMBER(BufferStructCount);
  SERIALISE_MEMBER(FirstElement);
  SERIALISE_MEMBER(NumElements);

  SERIALISE_MEMBER(Flags);
  SERIALISE_MEMBER(HighestMip);
  SERIALISE_MEMBER(NumMipLevels);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(FirstArraySlice);

  SIZE_CHECK(64);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Sampler &el)
{
  SERIALISE_MEMBER(Samp);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(customName);
  SERIALISE_MEMBER(AddressU);
  SERIALISE_MEMBER(AddressV);
  SERIALISE_MEMBER(AddressW);
  SERIALISE_MEMBER(BorderColor);
  SERIALISE_MEMBER(Comparison);
  SERIALISE_MEMBER(Filter);
  SERIALISE_MEMBER(MaxAniso);
  SERIALISE_MEMBER(MaxLOD);
  SERIALISE_MEMBER(MinLOD);
  SERIALISE_MEMBER(MipLODBias);

  SIZE_CHECK(96);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::CBuffer &el)
{
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(VecOffset);
  SERIALISE_MEMBER(VecCount);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Shader &el)
{
  SERIALISE_MEMBER(Object);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(customName);
  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(SRVs);
  SERIALISE_MEMBER(UAVs);
  SERIALISE_MEMBER(Samplers);
  SERIALISE_MEMBER(ConstantBuffers);
  SERIALISE_MEMBER(ClassInstances);

  if(ser.IsReading())
    el.ShaderDetails = NULL;
  SERIALISE_MEMBER(BindpointMapping);

  SIZE_CHECK(208);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::SOBind &el)
{
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(Offset);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::SO &el)
{
  SERIALISE_MEMBER(Outputs);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Viewport &el)
{
  SERIALISE_MEMBER(X);
  SERIALISE_MEMBER(Y);
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(MinDepth);
  SERIALISE_MEMBER(MaxDepth);
  SERIALISE_MEMBER(Enabled);

  SIZE_CHECK(28);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Scissor &el)
{
  SERIALISE_MEMBER(left);
  SERIALISE_MEMBER(top);
  SERIALISE_MEMBER(right);
  SERIALISE_MEMBER(bottom);
  SERIALISE_MEMBER(Enabled);

  SIZE_CHECK(20);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::RasterizerState &el)
{
  SERIALISE_MEMBER(State);
  SERIALISE_MEMBER(fillMode);
  SERIALISE_MEMBER(cullMode);
  SERIALISE_MEMBER(FrontCCW);
  SERIALISE_MEMBER(DepthBias);
  SERIALISE_MEMBER(DepthBiasClamp);
  SERIALISE_MEMBER(SlopeScaledDepthBias);
  SERIALISE_MEMBER(DepthClip);
  SERIALISE_MEMBER(ScissorEnable);
  SERIALISE_MEMBER(MultisampleEnable);
  SERIALISE_MEMBER(AntialiasedLineEnable);
  SERIALISE_MEMBER(ForcedSampleCount);
  SERIALISE_MEMBER(ConservativeRasterization);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Rasterizer &el)
{
  SERIALISE_MEMBER(Viewports);
  SERIALISE_MEMBER(Scissors);
  SERIALISE_MEMBER(m_State);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::StencilFace &el)
{
  SERIALISE_MEMBER(FailOp);
  SERIALISE_MEMBER(DepthFailOp);
  SERIALISE_MEMBER(PassOp);
  SERIALISE_MEMBER(Func);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::DepthStencilState &el)
{
  SERIALISE_MEMBER(State);
  SERIALISE_MEMBER(DepthEnable);
  SERIALISE_MEMBER(DepthFunc);
  SERIALISE_MEMBER(DepthWrites);
  SERIALISE_MEMBER(StencilEnable);
  SERIALISE_MEMBER(StencilReadMask);
  SERIALISE_MEMBER(StencilWriteMask);
  SERIALISE_MEMBER(m_FrontFace);
  SERIALISE_MEMBER(m_BackFace);
  SERIALISE_MEMBER(StencilRef);

  SIZE_CHECK(56);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::BlendEquation &el)
{
  SERIALISE_MEMBER(Source);
  SERIALISE_MEMBER(Destination);
  SERIALISE_MEMBER(Operation);

  SIZE_CHECK(12);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Blend &el)
{
  SERIALISE_MEMBER(m_Blend);
  SERIALISE_MEMBER(m_AlphaBlend);

  SERIALISE_MEMBER(Logic);

  SERIALISE_MEMBER(Enabled);
  SERIALISE_MEMBER(LogicEnabled);
  SERIALISE_MEMBER(WriteMask);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::BlendState &el)
{
  SERIALISE_MEMBER(State);
  SERIALISE_MEMBER(AlphaToCoverage);
  SERIALISE_MEMBER(IndependentBlend);
  SERIALISE_MEMBER(Blends);
  SERIALISE_MEMBER(BlendFactor);

  SERIALISE_MEMBER(SampleMask);

  SIZE_CHECK(56);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::OM &el)
{
  SERIALISE_MEMBER(m_State);
  SERIALISE_MEMBER(m_BlendState);
  SERIALISE_MEMBER(RenderTargets);
  SERIALISE_MEMBER(UAVStartSlot);
  SERIALISE_MEMBER(UAVs);
  SERIALISE_MEMBER(DepthTarget);
  SERIALISE_MEMBER(DepthReadOnly);
  SERIALISE_MEMBER(StencilReadOnly);

  SIZE_CHECK(224);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::State &el)
{
  SERIALISE_MEMBER(m_IA);

  SERIALISE_MEMBER(m_VS);
  SERIALISE_MEMBER(m_HS);
  SERIALISE_MEMBER(m_DS);
  SERIALISE_MEMBER(m_GS);
  SERIALISE_MEMBER(m_PS);
  SERIALISE_MEMBER(m_CS);

  SERIALISE_MEMBER(m_SO);

  SERIALISE_MEMBER(m_RS);
  SERIALISE_MEMBER(m_OM);

  SIZE_CHECK(1656);
}

#pragma endregion D3D11 pipeline state

#pragma region D3D12 pipeline state

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Layout &el)
{
  SERIALISE_MEMBER(SemanticName);
  SERIALISE_MEMBER(SemanticIndex);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(InputSlot);
  SERIALISE_MEMBER(ByteOffset);
  SERIALISE_MEMBER(PerInstance);
  SERIALISE_MEMBER(InstanceDataStepRate);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::VB &el)
{
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(Offset);
  SERIALISE_MEMBER(Size);
  SERIALISE_MEMBER(Stride);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::IB &el)
{
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(Offset);
  SERIALISE_MEMBER(Size);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::IA &el)
{
  SERIALISE_MEMBER(layouts);
  SERIALISE_MEMBER(vbuffers);
  SERIALISE_MEMBER(ibuffer);

  SERIALISE_MEMBER(indexStripCutValue);

  SIZE_CHECK(64);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::View &el)
{
  SERIALISE_MEMBER(Immediate);
  SERIALISE_MEMBER(RootElement);
  SERIALISE_MEMBER(TableIndex);
  SERIALISE_MEMBER(Resource);
  SERIALISE_MEMBER(Type);
  SERIALISE_MEMBER(Format);

  SERIALISE_MEMBER(swizzle);
  SERIALISE_MEMBER(BufferFlags);
  SERIALISE_MEMBER(BufferStructCount);
  SERIALISE_MEMBER(ElementSize);
  SERIALISE_MEMBER(FirstElement);
  SERIALISE_MEMBER(NumElements);

  SERIALISE_MEMBER(CounterResource);
  SERIALISE_MEMBER(CounterByteOffset);

  SERIALISE_MEMBER(HighestMip);
  SERIALISE_MEMBER(NumMipLevels);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(FirstArraySlice);

  SERIALISE_MEMBER(MinLODClamp);

  SIZE_CHECK(120);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Sampler &el)
{
  SERIALISE_MEMBER(Immediate);
  SERIALISE_MEMBER(RootElement);
  SERIALISE_MEMBER(TableIndex);
  SERIALISE_MEMBER(AddressU);
  SERIALISE_MEMBER(AddressV);
  SERIALISE_MEMBER(AddressW);
  SERIALISE_MEMBER(BorderColor);
  SERIALISE_MEMBER(Comparison);
  SERIALISE_MEMBER(Filter);
  SERIALISE_MEMBER(MaxAniso);
  SERIALISE_MEMBER(MaxLOD);
  SERIALISE_MEMBER(MinLOD);
  SERIALISE_MEMBER(MipLODBias);

  SIZE_CHECK(76);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::CBuffer &el)
{
  SERIALISE_MEMBER(Immediate);
  SERIALISE_MEMBER(RootElement);
  SERIALISE_MEMBER(TableIndex);
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(Offset);
  SERIALISE_MEMBER(ByteSize);
  SERIALISE_MEMBER(RootValues);

  SIZE_CHECK(56);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::RegisterSpace &el)
{
  SERIALISE_MEMBER(ConstantBuffers);
  SERIALISE_MEMBER(Samplers);
  SERIALISE_MEMBER(SRVs);
  SERIALISE_MEMBER(UAVs);

  SIZE_CHECK(64);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Shader &el)
{
  SERIALISE_MEMBER(Object);
  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(Spaces);

  if(ser.IsReading())
    el.ShaderDetails = NULL;
  SERIALISE_MEMBER(BindpointMapping);

  SIZE_CHECK(120);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::SOBind &el)
{
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(Offset);
  SERIALISE_MEMBER(Size);
  SERIALISE_MEMBER(WrittenCountBuffer);
  SERIALISE_MEMBER(WrittenCountOffset);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Streamout &el)
{
  SERIALISE_MEMBER(Outputs);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Viewport &el)
{
  SERIALISE_MEMBER(X);
  SERIALISE_MEMBER(Y);
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(MinDepth);
  SERIALISE_MEMBER(MaxDepth);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Scissor &el)
{
  SERIALISE_MEMBER(left);
  SERIALISE_MEMBER(top);
  SERIALISE_MEMBER(right);
  SERIALISE_MEMBER(bottom);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::RasterizerState &el)
{
  SERIALISE_MEMBER(fillMode);
  SERIALISE_MEMBER(cullMode);
  SERIALISE_MEMBER(FrontCCW);
  SERIALISE_MEMBER(DepthBias);
  SERIALISE_MEMBER(DepthBiasClamp);
  SERIALISE_MEMBER(SlopeScaledDepthBias);
  SERIALISE_MEMBER(DepthClip);
  SERIALISE_MEMBER(MultisampleEnable);
  SERIALISE_MEMBER(AntialiasedLineEnable);
  SERIALISE_MEMBER(ForcedSampleCount);
  SERIALISE_MEMBER(ConservativeRasterization);

  SIZE_CHECK(36);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Rasterizer &el)
{
  SERIALISE_MEMBER(SampleMask);
  SERIALISE_MEMBER(Viewports);
  SERIALISE_MEMBER(Scissors);
  SERIALISE_MEMBER(m_State);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::StencilFace &el)
{
  SERIALISE_MEMBER(FailOp);
  SERIALISE_MEMBER(DepthFailOp);
  SERIALISE_MEMBER(PassOp);
  SERIALISE_MEMBER(Func);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::DepthStencilState &el)
{
  SERIALISE_MEMBER(DepthEnable);
  SERIALISE_MEMBER(DepthWrites);
  SERIALISE_MEMBER(DepthFunc);
  SERIALISE_MEMBER(StencilEnable);
  SERIALISE_MEMBER(StencilReadMask);
  SERIALISE_MEMBER(StencilWriteMask);
  SERIALISE_MEMBER(m_FrontFace);
  SERIALISE_MEMBER(m_BackFace);
  SERIALISE_MEMBER(StencilRef);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::BlendEquation &el)
{
  SERIALISE_MEMBER(Source);
  SERIALISE_MEMBER(Destination);
  SERIALISE_MEMBER(Operation);

  SIZE_CHECK(12);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Blend &el)
{
  SERIALISE_MEMBER(m_Blend);
  SERIALISE_MEMBER(m_AlphaBlend);

  SERIALISE_MEMBER(Logic);

  SERIALISE_MEMBER(Enabled);
  SERIALISE_MEMBER(LogicEnabled);
  SERIALISE_MEMBER(WriteMask);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::BlendState &el)
{
  SERIALISE_MEMBER(AlphaToCoverage);
  SERIALISE_MEMBER(IndependentBlend);
  SERIALISE_MEMBER(Blends);
  SERIALISE_MEMBER(BlendFactor);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::OM &el)
{
  SERIALISE_MEMBER(m_State);
  SERIALISE_MEMBER(m_BlendState);

  SERIALISE_MEMBER(RenderTargets);
  SERIALISE_MEMBER(DepthTarget);
  SERIALISE_MEMBER(DepthReadOnly);
  SERIALISE_MEMBER(StencilReadOnly);

  SERIALISE_MEMBER(multiSampleCount);
  SERIALISE_MEMBER(multiSampleQuality);

  SIZE_CHECK(240);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::ResourceState &el)
{
  SERIALISE_MEMBER(name);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::ResourceData &el)
{
  SERIALISE_MEMBER(id);
  SERIALISE_MEMBER(states);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::State &el)
{
  SERIALISE_MEMBER(pipeline);
  SERIALISE_MEMBER(customName);
  SERIALISE_MEMBER(name);

  SERIALISE_MEMBER(rootSig);

  SERIALISE_MEMBER(m_IA);

  SERIALISE_MEMBER(m_VS);
  SERIALISE_MEMBER(m_HS);
  SERIALISE_MEMBER(m_DS);
  SERIALISE_MEMBER(m_GS);
  SERIALISE_MEMBER(m_PS);
  SERIALISE_MEMBER(m_CS);

  SERIALISE_MEMBER(m_SO);

  SERIALISE_MEMBER(m_RS);

  SERIALISE_MEMBER(m_OM);

  SERIALISE_MEMBER(Resources);

  SIZE_CHECK(1176);
}

#pragma endregion D3D12 pipeline state

#pragma region OpenGL pipeline state

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::VertexAttribute &el)
{
  SERIALISE_MEMBER(Enabled);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(GenericValue);
  SERIALISE_MEMBER(BufferSlot);
  SERIALISE_MEMBER(RelativeOffset);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::VB &el)
{
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(Stride);
  SERIALISE_MEMBER(Offset);
  SERIALISE_MEMBER(Divisor);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::VertexInput &el)
{
  SERIALISE_MEMBER(attributes);
  SERIALISE_MEMBER(vbuffers);
  SERIALISE_MEMBER(ibuffer);
  SERIALISE_MEMBER(primitiveRestart);
  SERIALISE_MEMBER(restartIndex);
  SERIALISE_MEMBER(provokingVertexLast);

  SIZE_CHECK(56);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Shader &el)
{
  SERIALISE_MEMBER(Object);

  SERIALISE_MEMBER(ShaderName);
  SERIALISE_MEMBER(customShaderName);

  SERIALISE_MEMBER(ProgramName);
  SERIALISE_MEMBER(customProgramName);

  SERIALISE_MEMBER(PipelineActive);
  SERIALISE_MEMBER(PipelineName);
  SERIALISE_MEMBER(customPipelineName);

  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(Subroutines);

  if(ser.IsReading())
    el.ShaderDetails = NULL;
  SERIALISE_MEMBER(BindpointMapping);

  SIZE_CHECK(192);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::FixedVertexProcessing &el)
{
  SERIALISE_MEMBER(defaultInnerLevel);
  SERIALISE_MEMBER(defaultOuterLevel);
  SERIALISE_MEMBER(discard);
  SERIALISE_MEMBER(clipPlanes);
  SERIALISE_MEMBER(clipOriginLowerLeft);
  SERIALISE_MEMBER(clipNegativeOneToOne);

  SIZE_CHECK(36);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Texture &el)
{
  SERIALISE_MEMBER(Resource);
  SERIALISE_MEMBER(FirstSlice);
  SERIALISE_MEMBER(HighestMip);
  SERIALISE_MEMBER(ResType);
  SERIALISE_MEMBER(Swizzle);
  SERIALISE_MEMBER(DepthReadChannel);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Sampler &el)
{
  SERIALISE_MEMBER(Samp);
  SERIALISE_MEMBER(AddressS);
  SERIALISE_MEMBER(AddressT);
  SERIALISE_MEMBER(AddressR);
  SERIALISE_MEMBER(BorderColor);
  SERIALISE_MEMBER(Comparison);
  SERIALISE_MEMBER(Filter);
  SERIALISE_MEMBER(SeamlessCube);
  SERIALISE_MEMBER(MaxAniso);
  SERIALISE_MEMBER(MaxLOD);
  SERIALISE_MEMBER(MinLOD);
  SERIALISE_MEMBER(MipLODBias);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Buffer &el)
{
  SERIALISE_MEMBER(Resource);
  SERIALISE_MEMBER(Offset);
  SERIALISE_MEMBER(Size);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::ImageLoadStore &el)
{
  SERIALISE_MEMBER(Resource);
  SERIALISE_MEMBER(Level);
  SERIALISE_MEMBER(Layered);
  SERIALISE_MEMBER(Layer);
  SERIALISE_MEMBER(ResType);
  SERIALISE_MEMBER(readAllowed);
  SERIALISE_MEMBER(writeAllowed);
  SERIALISE_MEMBER(Format);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Feedback &el)
{
  SERIALISE_MEMBER(Obj);
  SERIALISE_MEMBER(BufferBinding);
  SERIALISE_MEMBER(Offset);
  SERIALISE_MEMBER(Size);
  SERIALISE_MEMBER(Active);
  SERIALISE_MEMBER(Paused);

  SIZE_CHECK(112);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Viewport &el)
{
  SERIALISE_MEMBER(Left);
  SERIALISE_MEMBER(Bottom);
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(MinDepth);
  SERIALISE_MEMBER(MaxDepth);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Scissor &el)
{
  SERIALISE_MEMBER(Left);
  SERIALISE_MEMBER(Bottom);
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(Enabled);

  SIZE_CHECK(20);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::RasterizerState &el)
{
  SERIALISE_MEMBER(fillMode);
  SERIALISE_MEMBER(cullMode);
  SERIALISE_MEMBER(FrontCCW);
  SERIALISE_MEMBER(DepthBias);
  SERIALISE_MEMBER(SlopeScaledDepthBias);
  SERIALISE_MEMBER(OffsetClamp);
  SERIALISE_MEMBER(DepthClamp);

  SERIALISE_MEMBER(MultisampleEnable);
  SERIALISE_MEMBER(SampleShading);
  SERIALISE_MEMBER(SampleMask);
  SERIALISE_MEMBER(SampleMaskValue);
  SERIALISE_MEMBER(SampleCoverage);
  SERIALISE_MEMBER(SampleCoverageInvert);
  SERIALISE_MEMBER(SampleCoverageValue);
  SERIALISE_MEMBER(SampleAlphaToCoverage);
  SERIALISE_MEMBER(SampleAlphaToOne);
  SERIALISE_MEMBER(MinSampleShadingRate);

  SERIALISE_MEMBER(ProgrammablePointSize);
  SERIALISE_MEMBER(PointSize);
  SERIALISE_MEMBER(LineWidth);
  SERIALISE_MEMBER(PointFadeThreshold);
  SERIALISE_MEMBER(PointOriginUpperLeft);

  SIZE_CHECK(68);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Rasterizer &el)
{
  SERIALISE_MEMBER(Viewports);
  SERIALISE_MEMBER(Scissors);
  SERIALISE_MEMBER(m_State);

  SIZE_CHECK(104);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::DepthState &el)
{
  SERIALISE_MEMBER(DepthEnable);
  SERIALISE_MEMBER(DepthFunc);
  SERIALISE_MEMBER(DepthWrites);
  SERIALISE_MEMBER(DepthBounds);
  SERIALISE_MEMBER(NearBound);
  SERIALISE_MEMBER(FarBound);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::StencilFace &el)
{
  SERIALISE_MEMBER(FailOp);
  SERIALISE_MEMBER(DepthFailOp);
  SERIALISE_MEMBER(PassOp);
  SERIALISE_MEMBER(Func);
  SERIALISE_MEMBER(Ref);
  SERIALISE_MEMBER(ValueMask);
  SERIALISE_MEMBER(WriteMask);

  SIZE_CHECK(20);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::StencilState &el)
{
  SERIALISE_MEMBER(StencilEnable);
  SERIALISE_MEMBER(m_FrontFace);
  SERIALISE_MEMBER(m_BackFace);

  SIZE_CHECK(44);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Attachment &el)
{
  SERIALISE_MEMBER(Obj);
  SERIALISE_MEMBER(Layer);
  SERIALISE_MEMBER(Mip);
  SERIALISE_MEMBER(Swizzle);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::FBO &el)
{
  SERIALISE_MEMBER(Obj);
  SERIALISE_MEMBER(Color);
  SERIALISE_MEMBER(Depth);
  SERIALISE_MEMBER(Stencil);
  SERIALISE_MEMBER(DrawBuffers);
  SERIALISE_MEMBER(ReadBuffer);

  SIZE_CHECK(112);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::BlendEquation &el)
{
  SERIALISE_MEMBER(Source);
  SERIALISE_MEMBER(Destination);
  SERIALISE_MEMBER(Operation);

  SIZE_CHECK(12);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Blend &el)
{
  SERIALISE_MEMBER(m_Blend);
  SERIALISE_MEMBER(m_AlphaBlend);
  SERIALISE_MEMBER(Logic);
  SERIALISE_MEMBER(Enabled);
  SERIALISE_MEMBER(WriteMask);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::BlendState &el)
{
  SERIALISE_MEMBER(BlendFactor);
  SERIALISE_MEMBER(Blends);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::FrameBuffer &el)
{
  SERIALISE_MEMBER(FramebufferSRGB);
  SERIALISE_MEMBER(Dither);
  SERIALISE_MEMBER(m_DrawFBO);
  SERIALISE_MEMBER(m_ReadFBO);
  SERIALISE_MEMBER(m_Blending);

  SIZE_CHECK(264);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Hints &el)
{
  SERIALISE_MEMBER(Derivatives);
  SERIALISE_MEMBER(LineSmooth);
  SERIALISE_MEMBER(PolySmooth);
  SERIALISE_MEMBER(TexCompression);
  SERIALISE_MEMBER(LineSmoothEnabled);
  SERIALISE_MEMBER(PolySmoothEnabled);

  SIZE_CHECK(20);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::State &el)
{
  SERIALISE_MEMBER(m_VtxIn);

  SERIALISE_MEMBER(m_VS);
  SERIALISE_MEMBER(m_TCS);
  SERIALISE_MEMBER(m_TES);
  SERIALISE_MEMBER(m_GS);
  SERIALISE_MEMBER(m_FS);
  SERIALISE_MEMBER(m_CS);

  SERIALISE_MEMBER(m_VtxProcess);

  SERIALISE_MEMBER(Textures);
  SERIALISE_MEMBER(Samplers);
  SERIALISE_MEMBER(AtomicBuffers);
  SERIALISE_MEMBER(UniformBuffers);
  SERIALISE_MEMBER(ShaderStorageBuffers);
  SERIALISE_MEMBER(Images);

  SERIALISE_MEMBER(m_Feedback);

  SERIALISE_MEMBER(m_Rasterizer);
  SERIALISE_MEMBER(m_DepthState);
  SERIALISE_MEMBER(m_StencilState);

  SERIALISE_MEMBER(m_FB);

  SERIALISE_MEMBER(m_Hints);

  SIZE_CHECK(1928);
}

#pragma endregion OpenGL pipeline state

#pragma region Vulkan pipeline state

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::BindingElement &el)
{
  SERIALISE_MEMBER(view);
  SERIALISE_MEMBER(res);
  SERIALISE_MEMBER(sampler);
  SERIALISE_MEMBER(immutableSampler);

  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(customName);

  SERIALISE_MEMBER(viewfmt);
  SERIALISE_MEMBER(swizzle);
  SERIALISE_MEMBER(baseMip);
  SERIALISE_MEMBER(baseLayer);
  SERIALISE_MEMBER(numMip);
  SERIALISE_MEMBER(numLayer);

  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(size);

  SERIALISE_MEMBER(Filter);
  SERIALISE_MEMBER(AddressU);
  SERIALISE_MEMBER(AddressV);
  SERIALISE_MEMBER(AddressW);
  SERIALISE_MEMBER(mipBias);
  SERIALISE_MEMBER(maxAniso);
  SERIALISE_MEMBER(comparison);
  SERIALISE_MEMBER(minlod);
  SERIALISE_MEMBER(maxlod);
  SERIALISE_MEMBER(BorderColor);
  SERIALISE_MEMBER(unnormalized);

  SIZE_CHECK(176);
};

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::DescriptorBinding &el)
{
  SERIALISE_MEMBER(descriptorCount);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(stageFlags);

  SERIALISE_MEMBER(binds);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::DescriptorSet &el)
{
  SERIALISE_MEMBER(layout);
  SERIALISE_MEMBER(descset);

  SERIALISE_MEMBER(bindings);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Pipeline &el)
{
  SERIALISE_MEMBER(obj);
  SERIALISE_MEMBER(flags);

  SERIALISE_MEMBER(DescSets);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::IB &el)
{
  SERIALISE_MEMBER(buf);
  SERIALISE_MEMBER(offs);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::InputAssembly &el)
{
  SERIALISE_MEMBER(primitiveRestartEnable);
  SERIALISE_MEMBER(ibuffer);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::VertexAttribute &el)
{
  SERIALISE_MEMBER(location);
  SERIALISE_MEMBER(binding);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(byteoffset);

  SIZE_CHECK(20);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::VertexBinding &el)
{
  SERIALISE_MEMBER(vbufferBinding);
  SERIALISE_MEMBER(bytestride);
  SERIALISE_MEMBER(perInstance);

  SIZE_CHECK(12);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::VB &el)
{
  SERIALISE_MEMBER(buffer);
  SERIALISE_MEMBER(offset);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::VertexInput &el)
{
  SERIALISE_MEMBER(attrs);
  SERIALISE_MEMBER(binds);
  SERIALISE_MEMBER(vbuffers);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::SpecInfo &el)
{
  SERIALISE_MEMBER(specID);
  SERIALISE_MEMBER(data);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Shader &el)
{
  SERIALISE_MEMBER(Object);
  SERIALISE_MEMBER(entryPoint);

  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(customName);
  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(specialization);

  if(ser.IsReading())
    el.ShaderDetails = NULL;
  SERIALISE_MEMBER(BindpointMapping);

  SIZE_CHECK(160);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Tessellation &el)
{
  SERIALISE_MEMBER(numControlPoints);

  SIZE_CHECK(4);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Viewport &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(minDepth);
  SERIALISE_MEMBER(maxDepth);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Scissor &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::ViewportScissor &el)
{
  SERIALISE_MEMBER(vp);
  SERIALISE_MEMBER(scissor);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::ViewState &el)
{
  SERIALISE_MEMBER(viewportScissors);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Raster &el)
{
  SERIALISE_MEMBER(depthClampEnable);
  SERIALISE_MEMBER(rasterizerDiscardEnable);
  SERIALISE_MEMBER(FrontCCW);
  SERIALISE_MEMBER(fillMode);
  SERIALISE_MEMBER(cullMode);

  SERIALISE_MEMBER(depthBias);
  SERIALISE_MEMBER(depthBiasClamp);
  SERIALISE_MEMBER(slopeScaledDepthBias);
  SERIALISE_MEMBER(lineWidth);

  SIZE_CHECK(28);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::MultiSample &el)
{
  SERIALISE_MEMBER(rasterSamples);
  SERIALISE_MEMBER(sampleShadingEnable);
  SERIALISE_MEMBER(minSampleShading);
  SERIALISE_MEMBER(sampleMask);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::BlendEquation &el)
{
  SERIALISE_MEMBER(Source);
  SERIALISE_MEMBER(Destination);
  SERIALISE_MEMBER(Operation);

  SIZE_CHECK(12);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Blend &el)
{
  SERIALISE_MEMBER(blendEnable);
  SERIALISE_MEMBER(blend);
  SERIALISE_MEMBER(alphaBlend);
  SERIALISE_MEMBER(writeMask);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::ColorBlend &el)
{
  SERIALISE_MEMBER(alphaToCoverageEnable);
  SERIALISE_MEMBER(alphaToOneEnable);
  SERIALISE_MEMBER(logicOpEnable);
  SERIALISE_MEMBER(logic);

  SERIALISE_MEMBER(attachments);

  SERIALISE_MEMBER(blendConst);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::StencilFace &el)
{
  SERIALISE_MEMBER(FailOp);
  SERIALISE_MEMBER(DepthFailOp);
  SERIALISE_MEMBER(PassOp);
  SERIALISE_MEMBER(Func);
  SERIALISE_MEMBER(ref);
  SERIALISE_MEMBER(compareMask);
  SERIALISE_MEMBER(writeMask);

  SIZE_CHECK(28);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::DepthStencil &el)
{
  SERIALISE_MEMBER(depthTestEnable);
  SERIALISE_MEMBER(depthWriteEnable);
  SERIALISE_MEMBER(depthBoundsEnable);
  SERIALISE_MEMBER(depthCompareOp);

  SERIALISE_MEMBER(stencilTestEnable);

  SERIALISE_MEMBER(front);
  SERIALISE_MEMBER(back);

  SERIALISE_MEMBER(minDepthBounds);
  SERIALISE_MEMBER(maxDepthBounds);

  SIZE_CHECK(76);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::RenderPass &el)
{
  SERIALISE_MEMBER(obj);
  SERIALISE_MEMBER(inputAttachments);
  SERIALISE_MEMBER(colorAttachments);
  SERIALISE_MEMBER(resolveAttachments);
  SERIALISE_MEMBER(depthstencilAttachment);

  SIZE_CHECK(64);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Attachment &el)
{
  SERIALISE_MEMBER(view);
  SERIALISE_MEMBER(img);

  SERIALISE_MEMBER(viewfmt);
  SERIALISE_MEMBER(swizzle);

  SERIALISE_MEMBER(baseMip);
  SERIALISE_MEMBER(baseLayer);
  SERIALISE_MEMBER(numMip);
  SERIALISE_MEMBER(numLayer);

  SIZE_CHECK(56);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Framebuffer &el)
{
  SERIALISE_MEMBER(obj);
  SERIALISE_MEMBER(attachments);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(layers);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::RenderArea &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::CurrentPass &el)
{
  SERIALISE_MEMBER(renderpass);
  SERIALISE_MEMBER(framebuffer);
  SERIALISE_MEMBER(renderArea);

  SIZE_CHECK(120);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::ImageLayout &el)
{
  SERIALISE_MEMBER(baseMip);
  SERIALISE_MEMBER(baseLayer);
  SERIALISE_MEMBER(numMip);
  SERIALISE_MEMBER(numLayer);
  SERIALISE_MEMBER(name);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::ImageData &el)
{
  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER(layouts);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::State &el)
{
  SERIALISE_MEMBER(compute);
  SERIALISE_MEMBER(graphics);

  SERIALISE_MEMBER(IA);
  SERIALISE_MEMBER(VI);

  SERIALISE_MEMBER(m_VS);
  SERIALISE_MEMBER(m_TCS);
  SERIALISE_MEMBER(m_TES);
  SERIALISE_MEMBER(m_GS);
  SERIALISE_MEMBER(m_FS);
  SERIALISE_MEMBER(m_CS);

  SERIALISE_MEMBER(Tess);

  SERIALISE_MEMBER(VP);
  SERIALISE_MEMBER(RS);
  SERIALISE_MEMBER(MSAA);
  SERIALISE_MEMBER(CB);
  SERIALISE_MEMBER(DS);
  SERIALISE_MEMBER(Pass);

  SERIALISE_MEMBER(images);

  SIZE_CHECK(1424);
}

#pragma endregion Vulkan pipeline state

INSTANTIATE_SERIALISE_TYPE(PathEntry)
INSTANTIATE_SERIALISE_TYPE(EnvironmentModification)
INSTANTIATE_SERIALISE_TYPE(CaptureOptions)
INSTANTIATE_SERIALISE_TYPE(ResourceFormat)
INSTANTIATE_SERIALISE_TYPE(BindpointMap)
INSTANTIATE_SERIALISE_TYPE(ShaderBindpointMapping)
INSTANTIATE_SERIALISE_TYPE(SigParameter)
INSTANTIATE_SERIALISE_TYPE(ShaderVariableType)
INSTANTIATE_SERIALISE_TYPE(ShaderConstant)
INSTANTIATE_SERIALISE_TYPE(ConstantBlock)
INSTANTIATE_SERIALISE_TYPE(ShaderSampler)
INSTANTIATE_SERIALISE_TYPE(ShaderResource)
INSTANTIATE_SERIALISE_TYPE(ShaderCompileFlags)
INSTANTIATE_SERIALISE_TYPE(ShaderDebugChunk)
INSTANTIATE_SERIALISE_TYPE(ShaderReflection)
INSTANTIATE_SERIALISE_TYPE(ShaderVariable)
INSTANTIATE_SERIALISE_TYPE(ShaderDebugState)
INSTANTIATE_SERIALISE_TYPE(ShaderDebugTrace)
INSTANTIATE_SERIALISE_TYPE(TextureDescription)
INSTANTIATE_SERIALISE_TYPE(BufferDescription)
INSTANTIATE_SERIALISE_TYPE(APIProperties)
INSTANTIATE_SERIALISE_TYPE(DebugMessage)
INSTANTIATE_SERIALISE_TYPE(APIEvent)
INSTANTIATE_SERIALISE_TYPE(DrawcallDescription)
INSTANTIATE_SERIALISE_TYPE(ConstantBindStats)
INSTANTIATE_SERIALISE_TYPE(SamplerBindStats)
INSTANTIATE_SERIALISE_TYPE(ResourceBindStats)
INSTANTIATE_SERIALISE_TYPE(ResourceUpdateStats)
INSTANTIATE_SERIALISE_TYPE(DrawcallStats)
INSTANTIATE_SERIALISE_TYPE(DispatchStats)
INSTANTIATE_SERIALISE_TYPE(IndexBindStats)
INSTANTIATE_SERIALISE_TYPE(VertexBindStats)
INSTANTIATE_SERIALISE_TYPE(LayoutBindStats)
INSTANTIATE_SERIALISE_TYPE(ShaderChangeStats)
INSTANTIATE_SERIALISE_TYPE(BlendStats)
INSTANTIATE_SERIALISE_TYPE(DepthStencilStats)
INSTANTIATE_SERIALISE_TYPE(RasterizationStats)
INSTANTIATE_SERIALISE_TYPE(OutputTargetStats)
INSTANTIATE_SERIALISE_TYPE(FrameStatistics)
INSTANTIATE_SERIALISE_TYPE(FrameDescription)
INSTANTIATE_SERIALISE_TYPE(FrameRecord)
INSTANTIATE_SERIALISE_TYPE(MeshFormat)
INSTANTIATE_SERIALISE_TYPE(FloatVector)
INSTANTIATE_SERIALISE_TYPE(Uuid)
INSTANTIATE_SERIALISE_TYPE(CounterDescription)
INSTANTIATE_SERIALISE_TYPE(PixelModification)
INSTANTIATE_SERIALISE_TYPE(EventUsage)
INSTANTIATE_SERIALISE_TYPE(CounterResult)
INSTANTIATE_SERIALISE_TYPE(CounterValue)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::Layout)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::IA)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::View)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::Sampler)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::Shader)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::Rasterizer)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::Blend)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::OM)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::State)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::Layout)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::IA)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::CBuffer)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::Sampler)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::View)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::RegisterSpace)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::Shader)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::Rasterizer)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::Blend)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::OM)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::ResourceState)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::ResourceData)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::State)
INSTANTIATE_SERIALISE_TYPE(GLPipe::VertexAttribute)
INSTANTIATE_SERIALISE_TYPE(GLPipe::VertexInput)
INSTANTIATE_SERIALISE_TYPE(GLPipe::Shader)
INSTANTIATE_SERIALISE_TYPE(GLPipe::Sampler)
INSTANTIATE_SERIALISE_TYPE(GLPipe::ImageLoadStore)
INSTANTIATE_SERIALISE_TYPE(GLPipe::Rasterizer)
INSTANTIATE_SERIALISE_TYPE(GLPipe::DepthState)
INSTANTIATE_SERIALISE_TYPE(GLPipe::StencilState)
INSTANTIATE_SERIALISE_TYPE(GLPipe::Blend)
INSTANTIATE_SERIALISE_TYPE(GLPipe::BlendState)
INSTANTIATE_SERIALISE_TYPE(GLPipe::Attachment)
INSTANTIATE_SERIALISE_TYPE(GLPipe::FrameBuffer)
INSTANTIATE_SERIALISE_TYPE(GLPipe::State)
INSTANTIATE_SERIALISE_TYPE(VKPipe::BindingElement)
INSTANTIATE_SERIALISE_TYPE(VKPipe::DescriptorBinding)
INSTANTIATE_SERIALISE_TYPE(VKPipe::DescriptorSet)
INSTANTIATE_SERIALISE_TYPE(VKPipe::Pipeline)
INSTANTIATE_SERIALISE_TYPE(VKPipe::VertexAttribute)
INSTANTIATE_SERIALISE_TYPE(VKPipe::VertexInput)
INSTANTIATE_SERIALISE_TYPE(VKPipe::SpecInfo)
INSTANTIATE_SERIALISE_TYPE(VKPipe::Shader)
INSTANTIATE_SERIALISE_TYPE(VKPipe::ViewState)
INSTANTIATE_SERIALISE_TYPE(VKPipe::Blend)
INSTANTIATE_SERIALISE_TYPE(VKPipe::ColorBlend)
INSTANTIATE_SERIALISE_TYPE(VKPipe::Attachment)
INSTANTIATE_SERIALISE_TYPE(VKPipe::DepthStencil)
INSTANTIATE_SERIALISE_TYPE(VKPipe::CurrentPass)
INSTANTIATE_SERIALISE_TYPE(VKPipe::ImageLayout)
INSTANTIATE_SERIALISE_TYPE(VKPipe::ImageData)
INSTANTIATE_SERIALISE_TYPE(VKPipe::State)