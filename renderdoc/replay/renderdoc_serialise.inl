/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
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
void DoSerialise(SerialiserType &ser, ExecuteResult &el)
{
  SERIALISE_MEMBER(status);
  SERIALISE_MEMBER(ident);

  SIZE_CHECK(8);
}

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
void DoSerialise(SerialiserType &ser, SectionProperties &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(flags);
  SERIALISE_MEMBER(version);
  SERIALISE_MEMBER(uncompressedSize);
  SERIALISE_MEMBER(compressedSize);

  SIZE_CHECK(48);
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
  SERIALISE_MEMBER(allowVSync);
  SERIALISE_MEMBER(allowFullscreen);
  SERIALISE_MEMBER(apiValidation);
  SERIALISE_MEMBER(captureCallstacks);
  SERIALISE_MEMBER(captureCallstacksOnlyDraws);
  SERIALISE_MEMBER(delayForDebugger);
  SERIALISE_MEMBER(verifyMapWrites);
  SERIALISE_MEMBER(hookIntoChildren);
  SERIALISE_MEMBER(refAllResources);
  SERIALISE_MEMBER(saveAllInitials);
  SERIALISE_MEMBER(captureAllCmdLists);
  SERIALISE_MEMBER(debugOutputMute);
  SERIALISE_MEMBER(queueCapturing);
  SERIALISE_MEMBER(queueCaptureStartFrame);
  SERIALISE_MEMBER(queueCaptureNumFrames);

  SIZE_CHECK(28);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ResourceFormat &el)
{
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(compType);
  SERIALISE_MEMBER(compCount);
  SERIALISE_MEMBER(compByteWidth);
  SERIALISE_MEMBER(bgraOrder);
  SERIALISE_MEMBER(srgbCorrected);

  SIZE_CHECK(6);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, Bindpoint &el)
{
  SERIALISE_MEMBER(bindset);
  SERIALISE_MEMBER(bind);
  SERIALISE_MEMBER(arraySize);
  SERIALISE_MEMBER(used);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderBindpointMapping &el)
{
  SERIALISE_MEMBER(inputAttributes);
  SERIALISE_MEMBER(constantBlocks);
  SERIALISE_MEMBER(samplers);
  SERIALISE_MEMBER(readOnlyResources);
  SERIALISE_MEMBER(readWriteResources);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, SigParameter &el)
{
  SERIALISE_MEMBER(varName);
  SERIALISE_MEMBER(semanticName);
  SERIALISE_MEMBER(semanticIdxName);
  SERIALISE_MEMBER(semanticIndex);
  SERIALISE_MEMBER(regIndex);
  SERIALISE_MEMBER(systemValue);
  SERIALISE_MEMBER(compType);
  SERIALISE_MEMBER(regChannelMask);
  SERIALISE_MEMBER(channelUsedMask);
  SERIALISE_MEMBER(needSemanticIndex);
  SERIALISE_MEMBER(compCount);
  SERIALISE_MEMBER(stream);
  SERIALISE_MEMBER(arrayIndex);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderVariableDescriptor &el)
{
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(rows);
  SERIALISE_MEMBER(columns);
  SERIALISE_MEMBER(rowMajorStorage);
  SERIALISE_MEMBER(elements);
  SERIALISE_MEMBER(arrayByteStride);
  SERIALISE_MEMBER(name);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderVariableType &el)
{
  SERIALISE_MEMBER(descriptor);
  SERIALISE_MEMBER(members);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderRegister &el)
{
  SERIALISE_MEMBER(vec);
  SERIALISE_MEMBER(comp);

  SIZE_CHECK(8);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderConstant &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(reg);
  SERIALISE_MEMBER(defaultValue);
  SERIALISE_MEMBER(type);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ConstantBlock &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(variables);
  SERIALISE_MEMBER(bindPoint);
  SERIALISE_MEMBER(byteSize);
  SERIALISE_MEMBER(bufferBacked);

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
  SERIALISE_MEMBER(resType);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(variableType);
  SERIALISE_MEMBER(bindPoint);
  SERIALISE_MEMBER(isTexture);
  SERIALISE_MEMBER(isReadOnly);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderEntryPoint &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(stage);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderCompileFlag &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(value);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderCompileFlags &el)
{
  SERIALISE_MEMBER(flags);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderSourceFile &el)
{
  SERIALISE_MEMBER(filename);
  SERIALISE_MEMBER(contents);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderDebugInfo &el)
{
  SERIALISE_MEMBER(compileFlags);
  SERIALISE_MEMBER(files);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderReflection &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(entryPoint);

  SERIALISE_MEMBER(stage);

  SERIALISE_MEMBER(debugInfo);

  SERIALISE_MEMBER(encoding);
  SERIALISE_MEMBER(rawBytes);

  SERIALISE_MEMBER(dispatchThreadsDimension);

  SERIALISE_MEMBER(inputSignature);
  SERIALISE_MEMBER(outputSignature);

  SERIALISE_MEMBER(constantBlocks);

  SERIALISE_MEMBER(samplers);

  SERIALISE_MEMBER(readOnlyResources);
  SERIALISE_MEMBER(readWriteResources);

  SERIALISE_MEMBER(interfaces);

  SIZE_CHECK(216);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ShaderVariable &el)
{
  SERIALISE_MEMBER(rows);
  SERIALISE_MEMBER(columns);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(type);

  SERIALISE_MEMBER(displayAsHex);
  SERIALISE_MEMBER(isStruct);
  SERIALISE_MEMBER(rowMajor);

  SERIALISE_MEMBER(value.u64v);

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
  SERIALISE_MEMBER(constantBlocks);
  SERIALISE_MEMBER(states);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, TextureFilter &el)
{
  SERIALISE_MEMBER(minify);
  SERIALISE_MEMBER(magnify);
  SERIALISE_MEMBER(mip);
  SERIALISE_MEMBER(filter);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ResourceDescription &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(autogeneratedName);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(initialisationChunks);
  SERIALISE_MEMBER(derivedResources);
  SERIALISE_MEMBER(parentResources);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, TextureDescription &el)
{
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(dimension);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(depth);
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(cubemap);
  SERIALISE_MEMBER(mips);
  SERIALISE_MEMBER(arraysize);
  SERIALISE_MEMBER(creationFlags);
  SERIALISE_MEMBER(msQual);
  SERIALISE_MEMBER(msSamp);
  SERIALISE_MEMBER(byteSize);

  SIZE_CHECK(72);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, BufferDescription &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(creationFlags);
  SERIALISE_MEMBER(length);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, APIProperties &el)
{
  SERIALISE_MEMBER(pipelineType);
  SERIALISE_MEMBER(localRenderer);
  SERIALISE_MEMBER(vendor);
  SERIALISE_MEMBER(degraded);
  SERIALISE_MEMBER(shadersMutable);

  SERIALISE_MEMBER(ShaderLinkage);
  SERIALISE_MEMBER(YUVTextures);
  SERIALISE_MEMBER(SparseResources);
  SERIALISE_MEMBER(MultiGPU);
  SERIALISE_MEMBER(D3D12Bundle);

  SIZE_CHECK(20);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DebugMessage &el)
{
  SERIALISE_MEMBER(eventId);
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
  SERIALISE_MEMBER(eventId);
  SERIALISE_MEMBER(callstack);
  SERIALISE_MEMBER(chunkIndex);
  SERIALISE_MEMBER(fileOffset);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DrawcallDescription &el)
{
  SERIALISE_MEMBER(eventId);
  SERIALISE_MEMBER(drawcallId);

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
  SERIALISE_MEMBER(indexResourceId);
  SERIALISE_MEMBER(indexByteOffset);
  SERIALISE_MEMBER(indexByteStride);
  SERIALISE_MEMBER(baseVertex);
  SERIALISE_MEMBER(vertexResourceId);
  SERIALISE_MEMBER(vertexByteOffset);
  SERIALISE_MEMBER(vertexByteStride);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(meshColor);
  SERIALISE_MEMBER(topology);
  SERIALISE_MEMBER(numIndices);
  SERIALISE_MEMBER(instStepRate);
  SERIALISE_MEMBER(nearPlane);
  SERIALISE_MEMBER(farPlane);
  SERIALISE_MEMBER(unproject);
  SERIALISE_MEMBER(instanced);
  SERIALISE_MEMBER(showAlpha);

  SIZE_CHECK(96);
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
  SERIALISE_MEMBER(words);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, CounterDescription &el)
{
  SERIALISE_MEMBER(counter);
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(category);
  SERIALISE_MEMBER(description);
  SERIALISE_MEMBER(resultType);
  SERIALISE_MEMBER(resultByteWidth);
  SERIALISE_MEMBER(unit);
  SERIALISE_MEMBER(uuid);

  SIZE_CHECK(88);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, PixelValue &el)
{
  SERIALISE_MEMBER(uintValue);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ModificationValue &el)
{
  SERIALISE_MEMBER(col);
  SERIALISE_MEMBER(depth);
  SERIALISE_MEMBER(stencil);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, PixelModification &el)
{
  SERIALISE_MEMBER(eventId);

  SERIALISE_MEMBER(directShaderWrite);
  SERIALISE_MEMBER(unboundPS);

  SERIALISE_MEMBER(fragIndex);
  SERIALISE_MEMBER(primitiveID);

  SERIALISE_MEMBER(preMod);
  SERIALISE_MEMBER(shaderOut);
  SERIALISE_MEMBER(postMod);

  SERIALISE_MEMBER(sampleMasked);
  SERIALISE_MEMBER(backfaceCulled);
  SERIALISE_MEMBER(depthClipped);
  SERIALISE_MEMBER(viewClipped);
  SERIALISE_MEMBER(scissorClipped);
  SERIALISE_MEMBER(shaderDiscarded);
  SERIALISE_MEMBER(depthTestFailed);
  SERIALISE_MEMBER(stencilTestFailed);
  SERIALISE_MEMBER(predicationSkipped);

  SIZE_CHECK(100);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, EventUsage &el)
{
  SERIALISE_MEMBER(eventId);
  SERIALISE_MEMBER(usage);
  SERIALISE_MEMBER(view);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, CounterResult &el)
{
  SERIALISE_MEMBER(eventId);
  SERIALISE_MEMBER(counter);
  SERIALISE_MEMBER(value);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, CounterValue &el)
{
  SERIALISE_MEMBER(u64);

  SIZE_CHECK(8);
}

#pragma region Common pipeline state

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, Viewport &el)
{
  SERIALISE_MEMBER(enabled);
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(minDepth);
  SERIALISE_MEMBER(maxDepth);

  SIZE_CHECK(28);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, Scissor &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(enabled);

  SIZE_CHECK(20);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, BlendEquation &el)
{
  SERIALISE_MEMBER(source);
  SERIALISE_MEMBER(destination);
  SERIALISE_MEMBER(operation);

  SIZE_CHECK(12);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ColorBlend &el)
{
  SERIALISE_MEMBER(colorBlend);
  SERIALISE_MEMBER(alphaBlend);

  SERIALISE_MEMBER(logicOperation);

  SERIALISE_MEMBER(enabled);
  SERIALISE_MEMBER(logicOperationEnabled);
  SERIALISE_MEMBER(writeMask);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, StencilFace &el)
{
  SERIALISE_MEMBER(failOperation);
  SERIALISE_MEMBER(depthFailOperation);
  SERIALISE_MEMBER(passOperation);
  SERIALISE_MEMBER(function);
  SERIALISE_MEMBER(reference);
  SERIALISE_MEMBER(compareMask);
  SERIALISE_MEMBER(writeMask);

  SIZE_CHECK(28);
}

#pragma endregion

#pragma region D3D11 pipeline state

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Layout &el)
{
  SERIALISE_MEMBER(semanticName);
  SERIALISE_MEMBER(semanticIndex);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(inputSlot);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(perInstance);
  SERIALISE_MEMBER(instanceDataStepRate);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::VertexBuffer &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(byteStride);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::IndexBuffer &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteOffset);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::InputAssembly &el)
{
  SERIALISE_MEMBER(layouts);
  SERIALISE_MEMBER(resourceId);
  // don't serialise bytecode, just set it to NULL. See the definition of SERIALISE_MEMBER_DUMMY
  SERIALISE_MEMBER_OPT_EMPTY(bytecode);
  SERIALISE_MEMBER(vertexBuffers);
  SERIALISE_MEMBER(indexBuffer);

  SIZE_CHECK(64);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::View &el)
{
  SERIALISE_MEMBER(viewResourceId);
  SERIALISE_MEMBER(resourceResourceId);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(viewFormat);

  SERIALISE_MEMBER(structured);
  SERIALISE_MEMBER(bufferStructCount);
  SERIALISE_MEMBER(elementByteSize);
  SERIALISE_MEMBER(firstElement);
  SERIALISE_MEMBER(numElements);

  SERIALISE_MEMBER(bufferFlags);
  SERIALISE_MEMBER(firstMip);
  SERIALISE_MEMBER(numMips);
  SERIALISE_MEMBER(firstSlice);
  SERIALISE_MEMBER(numSlices);

  SIZE_CHECK(64);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Sampler &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(addressU);
  SERIALISE_MEMBER(addressV);
  SERIALISE_MEMBER(addressW);
  SERIALISE_MEMBER(borderColor);
  SERIALISE_MEMBER(compareFunction);
  SERIALISE_MEMBER(filter);
  SERIALISE_MEMBER(maxAnisotropy);
  SERIALISE_MEMBER(maxLOD);
  SERIALISE_MEMBER(minLOD);
  SERIALISE_MEMBER(mipLODBias);

  SIZE_CHECK(72);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::ConstantBuffer &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(vecOffset);
  SERIALISE_MEMBER(vecCount);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Shader &el)
{
  SERIALISE_MEMBER(resourceId);
  // don't serialise reflection, just set it to NULL. See the definition of SERIALISE_MEMBER_DUMMY
  SERIALISE_MEMBER_OPT_EMPTY(reflection);
  SERIALISE_MEMBER(bindpointMapping);
  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(srvs);
  SERIALISE_MEMBER(uavs);
  SERIALISE_MEMBER(samplers);
  SERIALISE_MEMBER(constantBuffers);
  SERIALISE_MEMBER(classInstances);

  SIZE_CHECK(184);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::StreamOutBind &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteOffset);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::StreamOut &el)
{
  SERIALISE_MEMBER(outputs);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::RasterizerState &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(fillMode);
  SERIALISE_MEMBER(cullMode);
  SERIALISE_MEMBER(frontCCW);
  SERIALISE_MEMBER(depthBias);
  SERIALISE_MEMBER(depthBiasClamp);
  SERIALISE_MEMBER(slopeScaledDepthBias);
  SERIALISE_MEMBER(depthClip);
  SERIALISE_MEMBER(scissorEnable);
  SERIALISE_MEMBER(multisampleEnable);
  SERIALISE_MEMBER(antialiasedLines);
  SERIALISE_MEMBER(forcedSampleCount);
  SERIALISE_MEMBER(conservativeRasterization);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Rasterizer &el)
{
  SERIALISE_MEMBER(viewports);
  SERIALISE_MEMBER(scissors);
  SERIALISE_MEMBER(state);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::DepthStencilState &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(depthEnable);
  SERIALISE_MEMBER(depthFunction);
  SERIALISE_MEMBER(depthWrites);
  SERIALISE_MEMBER(stencilEnable);
  SERIALISE_MEMBER(frontFace);
  SERIALISE_MEMBER(backFace);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::BlendState &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(alphaToCoverage);
  SERIALISE_MEMBER(independentBlend);
  SERIALISE_MEMBER(blends);
  SERIALISE_MEMBER(blendFactor);
  SERIALISE_MEMBER(sampleMask);

  SIZE_CHECK(56);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::OutputMerger &el)
{
  SERIALISE_MEMBER(depthStencilState);
  SERIALISE_MEMBER(blendState);
  SERIALISE_MEMBER(renderTargets);
  SERIALISE_MEMBER(uavStartSlot);
  SERIALISE_MEMBER(uavs);
  SERIALISE_MEMBER(depthTarget);
  SERIALISE_MEMBER(depthReadOnly);
  SERIALISE_MEMBER(stencilReadOnly);

  SIZE_CHECK(248);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::Predication &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(value);
  SERIALISE_MEMBER(isPassing);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11Pipe::State &el)
{
  SERIALISE_MEMBER(inputAssembly);

  SERIALISE_MEMBER(vertexShader);
  SERIALISE_MEMBER(hullShader);
  SERIALISE_MEMBER(domainShader);
  SERIALISE_MEMBER(geometryShader);
  SERIALISE_MEMBER(pixelShader);
  SERIALISE_MEMBER(computeShader);

  SERIALISE_MEMBER(streamOut);

  SERIALISE_MEMBER(rasterizer);
  SERIALISE_MEMBER(outputMerger);

  SERIALISE_MEMBER(predication);

  SIZE_CHECK(1528);
}

#pragma endregion D3D11 pipeline state

#pragma region D3D12 pipeline state

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Layout &el)
{
  SERIALISE_MEMBER(semanticName);
  SERIALISE_MEMBER(semanticIndex);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(inputSlot);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(perInstance);
  SERIALISE_MEMBER(instanceDataStepRate);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::VertexBuffer &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(byteSize);
  SERIALISE_MEMBER(byteStride);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::IndexBuffer &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(byteSize);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::InputAssembly &el)
{
  SERIALISE_MEMBER(layouts);
  SERIALISE_MEMBER(vertexBuffers);
  SERIALISE_MEMBER(indexBuffer);

  SERIALISE_MEMBER(indexStripCutValue);

  SIZE_CHECK(64);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::View &el)
{
  SERIALISE_MEMBER(immediate);
  SERIALISE_MEMBER(rootElement);
  SERIALISE_MEMBER(tableIndex);
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(viewFormat);

  SERIALISE_MEMBER(swizzle);
  SERIALISE_MEMBER(bufferFlags);
  SERIALISE_MEMBER(bufferStructCount);
  SERIALISE_MEMBER(elementByteSize);
  SERIALISE_MEMBER(firstElement);
  SERIALISE_MEMBER(numElements);

  SERIALISE_MEMBER(counterResourceId);
  SERIALISE_MEMBER(counterByteOffset);

  SERIALISE_MEMBER(firstMip);
  SERIALISE_MEMBER(numMips);
  SERIALISE_MEMBER(firstSlice);
  SERIALISE_MEMBER(numSlices);

  SERIALISE_MEMBER(minLODClamp);

  SIZE_CHECK(120);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Sampler &el)
{
  SERIALISE_MEMBER(immediate);
  SERIALISE_MEMBER(rootElement);
  SERIALISE_MEMBER(tableIndex);
  SERIALISE_MEMBER(addressU);
  SERIALISE_MEMBER(addressV);
  SERIALISE_MEMBER(addressW);
  SERIALISE_MEMBER(borderColor);
  SERIALISE_MEMBER(compareFunction);
  SERIALISE_MEMBER(filter);
  SERIALISE_MEMBER(maxAnisotropy);
  SERIALISE_MEMBER(maxLOD);
  SERIALISE_MEMBER(minLOD);
  SERIALISE_MEMBER(mipLODBias);

  SIZE_CHECK(76);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::ConstantBuffer &el)
{
  SERIALISE_MEMBER(immediate);
  SERIALISE_MEMBER(rootElement);
  SERIALISE_MEMBER(tableIndex);
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(byteSize);
  SERIALISE_MEMBER(rootValues);

  SIZE_CHECK(56);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::RegisterSpace &el)
{
  SERIALISE_MEMBER(constantBuffers);
  SERIALISE_MEMBER(samplers);
  SERIALISE_MEMBER(srvs);
  SERIALISE_MEMBER(uavs);

  SIZE_CHECK(64);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Shader &el)
{
  SERIALISE_MEMBER(resourceId);
  // don't serialise reflection, just set it to NULL. See the definition of SERIALISE_MEMBER_DUMMY
  SERIALISE_MEMBER_OPT_EMPTY(reflection);
  SERIALISE_MEMBER(bindpointMapping);
  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(spaces);

  SIZE_CHECK(120);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::StreamOutBind &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(byteSize);
  SERIALISE_MEMBER(writtenCountResourceId);
  SERIALISE_MEMBER(writtenCountByteOffset);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::StreamOut &el)
{
  SERIALISE_MEMBER(outputs);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::RasterizerState &el)
{
  SERIALISE_MEMBER(fillMode);
  SERIALISE_MEMBER(cullMode);
  SERIALISE_MEMBER(frontCCW);
  SERIALISE_MEMBER(depthBias);
  SERIALISE_MEMBER(depthBiasClamp);
  SERIALISE_MEMBER(slopeScaledDepthBias);
  SERIALISE_MEMBER(depthClip);
  SERIALISE_MEMBER(multisampleEnable);
  SERIALISE_MEMBER(antialiasedLines);
  SERIALISE_MEMBER(forcedSampleCount);
  SERIALISE_MEMBER(conservativeRasterization);

  SIZE_CHECK(36);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::Rasterizer &el)
{
  SERIALISE_MEMBER(sampleMask);
  SERIALISE_MEMBER(viewports);
  SERIALISE_MEMBER(scissors);
  SERIALISE_MEMBER(state);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::DepthStencilState &el)
{
  SERIALISE_MEMBER(depthEnable);
  SERIALISE_MEMBER(depthWrites);
  SERIALISE_MEMBER(depthFunction);
  SERIALISE_MEMBER(stencilEnable);
  SERIALISE_MEMBER(frontFace);
  SERIALISE_MEMBER(backFace);

  SIZE_CHECK(68);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::BlendState &el)
{
  SERIALISE_MEMBER(alphaToCoverage);
  SERIALISE_MEMBER(independentBlend);
  SERIALISE_MEMBER(blends);
  SERIALISE_MEMBER(blendFactor);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::OM &el)
{
  SERIALISE_MEMBER(depthStencilState);
  SERIALISE_MEMBER(blendState);

  SERIALISE_MEMBER(renderTargets);
  SERIALISE_MEMBER(depthTarget);
  SERIALISE_MEMBER(depthReadOnly);
  SERIALISE_MEMBER(stencilReadOnly);

  SERIALISE_MEMBER(multiSampleCount);
  SERIALISE_MEMBER(multiSampleQuality);

  SIZE_CHECK(264);
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
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(states);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Pipe::State &el)
{
  SERIALISE_MEMBER(pipelineResourceId);
  SERIALISE_MEMBER(rootSignatureResourceId);

  SERIALISE_MEMBER(inputAssembly);

  SERIALISE_MEMBER(vertexShader);
  SERIALISE_MEMBER(hullShader);
  SERIALISE_MEMBER(domainShader);
  SERIALISE_MEMBER(geometryShader);
  SERIALISE_MEMBER(pixelShader);
  SERIALISE_MEMBER(computeShader);

  SERIALISE_MEMBER(streamOut);

  SERIALISE_MEMBER(rasterizer);

  SERIALISE_MEMBER(outputMerger);

  SERIALISE_MEMBER(resourceStates);

  SIZE_CHECK(1176);
}

#pragma endregion D3D12 pipeline state

#pragma region OpenGL pipeline state

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::VertexAttribute &el)
{
  SERIALISE_MEMBER(enabled);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(genericValue);
  SERIALISE_MEMBER(vertexBufferSlot);
  SERIALISE_MEMBER(byteOffset);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::VertexBuffer &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteStride);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(instanceDivisor);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::VertexInput &el)
{
  SERIALISE_MEMBER(attributes);
  SERIALISE_MEMBER(vertexBuffers);
  SERIALISE_MEMBER(indexBuffer);
  SERIALISE_MEMBER(primitiveRestart);
  SERIALISE_MEMBER(restartIndex);
  SERIALISE_MEMBER(provokingVertexLast);

  SIZE_CHECK(56);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Shader &el)
{
  SERIALISE_MEMBER(shaderResourceId);
  SERIALISE_MEMBER(programResourceId);

  // don't serialise reflection, just set it to NULL. See the definition of SERIALISE_MEMBER_DUMMY
  SERIALISE_MEMBER_OPT_EMPTY(reflection);
  SERIALISE_MEMBER(bindpointMapping);

  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(subroutines);

  SIZE_CHECK(128);
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
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(firstSlice);
  SERIALISE_MEMBER(firstMip);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(swizzle);
  SERIALISE_MEMBER(depthReadChannel);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Sampler &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(addressS);
  SERIALISE_MEMBER(addressT);
  SERIALISE_MEMBER(addressR);
  SERIALISE_MEMBER(borderColor);
  SERIALISE_MEMBER(compareFunction);
  SERIALISE_MEMBER(filter);
  SERIALISE_MEMBER(seamlessCubeMap);
  SERIALISE_MEMBER(maxAnisotropy);
  SERIALISE_MEMBER(maxLOD);
  SERIALISE_MEMBER(minLOD);
  SERIALISE_MEMBER(mipLODBias);

  SIZE_CHECK(80);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Buffer &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(byteSize);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::ImageLoadStore &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(mipLevel);
  SERIALISE_MEMBER(layered);
  SERIALISE_MEMBER(slice);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(readAllowed);
  SERIALISE_MEMBER(writeAllowed);
  SERIALISE_MEMBER(imageFormat);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Feedback &el)
{
  SERIALISE_MEMBER(feedbackResourceId);
  SERIALISE_MEMBER(bufferResourceId);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(byteSize);
  SERIALISE_MEMBER(active);
  SERIALISE_MEMBER(paused);

  SIZE_CHECK(112);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::RasterizerState &el)
{
  SERIALISE_MEMBER(fillMode);
  SERIALISE_MEMBER(cullMode);
  SERIALISE_MEMBER(frontCCW);
  SERIALISE_MEMBER(depthBias);
  SERIALISE_MEMBER(slopeScaledDepthBias);
  SERIALISE_MEMBER(offsetClamp);
  SERIALISE_MEMBER(depthClamp);

  SERIALISE_MEMBER(multisampleEnable);
  SERIALISE_MEMBER(sampleShading);
  SERIALISE_MEMBER(sampleMask);
  SERIALISE_MEMBER(sampleMaskValue);
  SERIALISE_MEMBER(sampleCoverage);
  SERIALISE_MEMBER(sampleCoverageInvert);
  SERIALISE_MEMBER(sampleCoverageValue);
  SERIALISE_MEMBER(alphaToCoverage);
  SERIALISE_MEMBER(alphaToOne);
  SERIALISE_MEMBER(minSampleShadingRate);

  SERIALISE_MEMBER(programmablePointSize);
  SERIALISE_MEMBER(pointSize);
  SERIALISE_MEMBER(lineWidth);
  SERIALISE_MEMBER(pointFadeThreshold);
  SERIALISE_MEMBER(pointOriginUpperLeft);

  SIZE_CHECK(68);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Rasterizer &el)
{
  SERIALISE_MEMBER(viewports);
  SERIALISE_MEMBER(scissors);
  SERIALISE_MEMBER(state);

  SIZE_CHECK(104);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::DepthState &el)
{
  SERIALISE_MEMBER(depthEnable);
  SERIALISE_MEMBER(depthFunction);
  SERIALISE_MEMBER(depthWrites);
  SERIALISE_MEMBER(depthBounds);
  SERIALISE_MEMBER(nearBound);
  SERIALISE_MEMBER(farBound);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::StencilState &el)
{
  SERIALISE_MEMBER(stencilEnable);
  SERIALISE_MEMBER(frontFace);
  SERIALISE_MEMBER(backFace);

  SIZE_CHECK(60);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Attachment &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(slice);
  SERIALISE_MEMBER(mipLevel);
  SERIALISE_MEMBER(swizzle);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::FBO &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(colorAttachments);
  SERIALISE_MEMBER(depthAttachment);
  SERIALISE_MEMBER(stencilAttachment);
  SERIALISE_MEMBER(drawBuffers);
  SERIALISE_MEMBER(readBuffer);

  SIZE_CHECK(112);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::BlendState &el)
{
  SERIALISE_MEMBER(blends);
  SERIALISE_MEMBER(blendFactor);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::FrameBuffer &el)
{
  SERIALISE_MEMBER(framebufferSRGB);
  SERIALISE_MEMBER(dither);
  SERIALISE_MEMBER(drawFBO);
  SERIALISE_MEMBER(readFBO);
  SERIALISE_MEMBER(blendState);

  SIZE_CHECK(264);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::Hints &el)
{
  SERIALISE_MEMBER(derivatives);
  SERIALISE_MEMBER(lineSmoothing);
  SERIALISE_MEMBER(polySmoothing);
  SERIALISE_MEMBER(textureCompression);
  SERIALISE_MEMBER(lineSmoothingEnabled);
  SERIALISE_MEMBER(polySmoothingEnabled);

  SIZE_CHECK(20);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLPipe::State &el)
{
  SERIALISE_MEMBER(vertexInput);

  SERIALISE_MEMBER(vertexShader);
  SERIALISE_MEMBER(tessControlShader);
  SERIALISE_MEMBER(tessEvalShader);
  SERIALISE_MEMBER(geometryShader);
  SERIALISE_MEMBER(fragmentShader);
  SERIALISE_MEMBER(computeShader);

  SERIALISE_MEMBER(pipelineResourceId);

  SERIALISE_MEMBER(vertexProcessing);

  SERIALISE_MEMBER(textures);
  SERIALISE_MEMBER(samplers);
  SERIALISE_MEMBER(atomicBuffers);
  SERIALISE_MEMBER(uniformBuffers);
  SERIALISE_MEMBER(shaderStorageBuffers);
  SERIALISE_MEMBER(images);

  SERIALISE_MEMBER(transformFeedback);

  SERIALISE_MEMBER(rasterizer);
  SERIALISE_MEMBER(depthState);
  SERIALISE_MEMBER(stencilState);

  SERIALISE_MEMBER(framebuffer);

  SERIALISE_MEMBER(hints);

  SIZE_CHECK(1568);
}

#pragma endregion OpenGL pipeline state

#pragma region Vulkan pipeline state

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::BindingElement &el)
{
  SERIALISE_MEMBER(viewResourceId);
  SERIALISE_MEMBER(resourceResourceId);
  SERIALISE_MEMBER(samplerResourceId);
  SERIALISE_MEMBER(immutableSampler);
  SERIALISE_MEMBER(viewFormat);
  SERIALISE_MEMBER(swizzle);
  SERIALISE_MEMBER(firstMip);
  SERIALISE_MEMBER(numMips);
  SERIALISE_MEMBER(firstSlice);
  SERIALISE_MEMBER(numSlices);

  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(byteSize);

  SERIALISE_MEMBER(filter);
  SERIALISE_MEMBER(addressU);
  SERIALISE_MEMBER(addressV);
  SERIALISE_MEMBER(addressW);
  SERIALISE_MEMBER(mipBias);
  SERIALISE_MEMBER(maxAnisotropy);
  SERIALISE_MEMBER(compareFunction);
  SERIALISE_MEMBER(minLOD);
  SERIALISE_MEMBER(maxLOD);
  SERIALISE_MEMBER(borderColor);
  SERIALISE_MEMBER(unnormalized);

  SIZE_CHECK(152);
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
  SERIALISE_MEMBER(layoutResourceId);
  SERIALISE_MEMBER(descriptorSetResourceId);

  SERIALISE_MEMBER(bindings);

  SIZE_CHECK(32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Pipeline &el)
{
  SERIALISE_MEMBER(pipelineResourceId);
  SERIALISE_MEMBER(pipelineLayoutResourceId);
  SERIALISE_MEMBER(flags);

  SERIALISE_MEMBER(descriptorSets);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::IndexBuffer &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteOffset);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::InputAssembly &el)
{
  SERIALISE_MEMBER(primitiveRestartEnable);
  SERIALISE_MEMBER(indexBuffer);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::VertexAttribute &el)
{
  SERIALISE_MEMBER(location);
  SERIALISE_MEMBER(binding);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(byteOffset);

  SIZE_CHECK(20);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::VertexBinding &el)
{
  SERIALISE_MEMBER(vertexBufferBinding);
  SERIALISE_MEMBER(byteStride);
  SERIALISE_MEMBER(perInstance);

  SIZE_CHECK(12);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::VertexBuffer &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(byteOffset);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::VertexInput &el)
{
  SERIALISE_MEMBER(attributes);
  SERIALISE_MEMBER(bindings);
  SERIALISE_MEMBER(vertexBuffers);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::SpecializationConstant &el)
{
  SERIALISE_MEMBER(specializationId);
  SERIALISE_MEMBER(data);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Shader &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(entryPoint);

  // don't serialise reflection, just set it to NULL. See the definition of SERIALISE_MEMBER_DUMMY
  SERIALISE_MEMBER_OPT_EMPTY(reflection);
  SERIALISE_MEMBER(bindpointMapping);

  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(specialization);

  SIZE_CHECK(136);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Tessellation &el)
{
  SERIALISE_MEMBER(numControlPoints);

  SIZE_CHECK(4);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::ViewportScissor &el)
{
  SERIALISE_MEMBER(vp);
  SERIALISE_MEMBER(scissor);

  SIZE_CHECK(48);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::ViewState &el)
{
  SERIALISE_MEMBER(viewportScissors);

  SIZE_CHECK(16);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Rasterizer &el)
{
  SERIALISE_MEMBER(depthClampEnable);
  SERIALISE_MEMBER(rasterizerDiscardEnable);
  SERIALISE_MEMBER(frontCCW);
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
void DoSerialise(SerialiserType &ser, VKPipe::ColorBlendState &el)
{
  SERIALISE_MEMBER(alphaToCoverageEnable);
  SERIALISE_MEMBER(alphaToOneEnable);

  SERIALISE_MEMBER(blends);

  SERIALISE_MEMBER(blendFactor);

  SIZE_CHECK(40);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::DepthStencil &el)
{
  SERIALISE_MEMBER(depthTestEnable);
  SERIALISE_MEMBER(depthWriteEnable);
  SERIALISE_MEMBER(depthBoundsEnable);
  SERIALISE_MEMBER(depthFunction);

  SERIALISE_MEMBER(stencilTestEnable);

  SERIALISE_MEMBER(frontFace);
  SERIALISE_MEMBER(backFace);

  SERIALISE_MEMBER(minDepthBounds);
  SERIALISE_MEMBER(maxDepthBounds);

  SIZE_CHECK(76);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::RenderPass &el)
{
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(subpass);
  SERIALISE_MEMBER(inputAttachments);
  SERIALISE_MEMBER(colorAttachments);
  SERIALISE_MEMBER(resolveAttachments);
  SERIALISE_MEMBER(depthstencilAttachment);

  SIZE_CHECK(72);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Attachment &el)
{
  SERIALISE_MEMBER(viewResourceId);
  SERIALISE_MEMBER(imageResourceId);

  SERIALISE_MEMBER(viewFormat);
  SERIALISE_MEMBER(swizzle);

  SERIALISE_MEMBER(firstMip);
  SERIALISE_MEMBER(firstSlice);
  SERIALISE_MEMBER(numMips);
  SERIALISE_MEMBER(numSlices);

  SIZE_CHECK(56);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::Framebuffer &el)
{
  SERIALISE_MEMBER(resourceId);
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

  SIZE_CHECK(128);
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
  SERIALISE_MEMBER(resourceId);
  SERIALISE_MEMBER(layouts);

  SIZE_CHECK(24);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VKPipe::State &el)
{
  SERIALISE_MEMBER(compute);
  SERIALISE_MEMBER(graphics);

  SERIALISE_MEMBER(inputAssembly);
  SERIALISE_MEMBER(vertexInput);

  SERIALISE_MEMBER(vertexShader);
  SERIALISE_MEMBER(tessControlShader);
  SERIALISE_MEMBER(tessEvalShader);
  SERIALISE_MEMBER(geometryShader);
  SERIALISE_MEMBER(fragmentShader);
  SERIALISE_MEMBER(computeShader);

  SERIALISE_MEMBER(tessellation);

  SERIALISE_MEMBER(viewportScissor);
  SERIALISE_MEMBER(rasterizer);
  SERIALISE_MEMBER(multisample);
  SERIALISE_MEMBER(colorBlend);
  SERIALISE_MEMBER(depthStencil);
  SERIALISE_MEMBER(currentPass);

  SERIALISE_MEMBER(images);

  SIZE_CHECK(1304);
}

#pragma endregion Vulkan pipeline state

INSTANTIATE_SERIALISE_TYPE(ExecuteResult)
INSTANTIATE_SERIALISE_TYPE(PathEntry)
INSTANTIATE_SERIALISE_TYPE(SectionProperties)
INSTANTIATE_SERIALISE_TYPE(EnvironmentModification)
INSTANTIATE_SERIALISE_TYPE(CaptureOptions)
INSTANTIATE_SERIALISE_TYPE(ResourceFormat)
INSTANTIATE_SERIALISE_TYPE(Bindpoint)
INSTANTIATE_SERIALISE_TYPE(ShaderBindpointMapping)
INSTANTIATE_SERIALISE_TYPE(SigParameter)
INSTANTIATE_SERIALISE_TYPE(ShaderVariableType)
INSTANTIATE_SERIALISE_TYPE(ShaderConstant)
INSTANTIATE_SERIALISE_TYPE(ConstantBlock)
INSTANTIATE_SERIALISE_TYPE(ShaderSampler)
INSTANTIATE_SERIALISE_TYPE(ShaderResource)
INSTANTIATE_SERIALISE_TYPE(ShaderEntryPoint)
INSTANTIATE_SERIALISE_TYPE(ShaderCompileFlags)
INSTANTIATE_SERIALISE_TYPE(ShaderDebugInfo)
INSTANTIATE_SERIALISE_TYPE(ShaderReflection)
INSTANTIATE_SERIALISE_TYPE(ShaderVariable)
INSTANTIATE_SERIALISE_TYPE(ShaderDebugState)
INSTANTIATE_SERIALISE_TYPE(ShaderDebugTrace)
INSTANTIATE_SERIALISE_TYPE(ResourceDescription)
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
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::InputAssembly)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::View)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::Sampler)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::Shader)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::Rasterizer)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::OutputMerger)
INSTANTIATE_SERIALISE_TYPE(D3D11Pipe::State)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::Layout)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::InputAssembly)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::ConstantBuffer)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::Sampler)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::View)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::RegisterSpace)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::Shader)
INSTANTIATE_SERIALISE_TYPE(D3D12Pipe::Rasterizer)
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
INSTANTIATE_SERIALISE_TYPE(VKPipe::SpecializationConstant)
INSTANTIATE_SERIALISE_TYPE(VKPipe::Shader)
INSTANTIATE_SERIALISE_TYPE(VKPipe::ViewState)
INSTANTIATE_SERIALISE_TYPE(VKPipe::ColorBlendState)
INSTANTIATE_SERIALISE_TYPE(VKPipe::Attachment)
INSTANTIATE_SERIALISE_TYPE(VKPipe::DepthStencil)
INSTANTIATE_SERIALISE_TYPE(VKPipe::CurrentPass)
INSTANTIATE_SERIALISE_TYPE(VKPipe::ImageLayout)
INSTANTIATE_SERIALISE_TYPE(VKPipe::ImageData)
INSTANTIATE_SERIALISE_TYPE(VKPipe::State)