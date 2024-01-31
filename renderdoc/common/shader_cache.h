/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#pragma once

#include <map>
#include "common/common.h"
#include "serialise/streamio.h"
#include "serialise/zstdio.h"

static const uint32_t ShaderCacheMagic = MAKE_FOURCC('R', 'D', '$', '$');

template <typename ResultType, typename ShaderCallbacks>
bool LoadShaderCache(const rdcstr &filename, const uint32_t magicNumber, const uint32_t versionNumber,
                     std::map<uint32_t, ResultType> &resultCache, const ShaderCallbacks &callbacks)
{
  rdcstr shadercache = FileIO::GetAppFolderFilename(filename);

  StreamReader fileReader(FileIO::fopen(shadercache, FileIO::ReadBinary));

  uint32_t globalMagic = 0, localMagic = 0, version = 0;
  fileReader.Read(globalMagic);
  fileReader.Read(localMagic);
  fileReader.Read(version);

  if(globalMagic != ShaderCacheMagic || localMagic != magicNumber || version != versionNumber)
    return false;

  uint64_t uncompressedSize = 0;
  fileReader.Read(uncompressedSize);

  // header has been read. The rest is zstd compressed
  StreamReader compressedReader(new ZSTDDecompressor(&fileReader, Ownership::Nothing),
                                uncompressedSize, Ownership::Stream);

  uint32_t numentries = 0;
  compressedReader.Read(numentries);

  bool ret = true;
  bytebuf data;

  for(uint32_t i = 0; i < numentries; i++)
  {
    uint32_t hash = 0, length = 0;
    compressedReader.Read(hash);
    compressedReader.Read(length);

    data.resize(length);
    compressedReader.Read(data.data(), length);

    ResultType result;
    bool created = callbacks.Create(length, data.data(), &result);

    if(!created)
    {
      RDCERR("Couldn't create blob of size %u from shadercache", length);
      ret = false;
      break;
    }

    resultCache[hash] = result;
  }

  return ret && !compressedReader.IsErrored() && !fileReader.IsErrored();
}

template <typename ResultType, typename ShaderCallbacks>
void SaveShaderCache(const rdcstr &filename, uint32_t magicNumber, uint32_t versionNumber,
                     const std::map<uint32_t, ResultType> &cache, const ShaderCallbacks &callbacks)
{
  rdcstr shadercache = FileIO::GetAppFolderFilename(filename);

  FILE *f = FileIO::fopen(shadercache, FileIO::WriteBinary);

  if(!f)
  {
    RDCERR("Error opening shader cache for write");
    return;
  }

  StreamWriter fileWriter(f, Ownership::Stream);

  fileWriter.Write(ShaderCacheMagic);
  fileWriter.Write(magicNumber);
  fileWriter.Write(versionNumber);

  uint32_t numentries = (uint32_t)cache.size();

  uint64_t uncompressedSize = sizeof(numentries);    // number of entries

  // hash + length + data for each entry
  for(auto it = cache.begin(); it != cache.end(); ++it)
    uncompressedSize += sizeof(uint32_t) * 2 + callbacks.GetSize(it->second);

  fileWriter.Write(uncompressedSize);

  StreamWriter compressedWriter(new ZSTDCompressor(&fileWriter, Ownership::Nothing),
                                Ownership::Stream);

  compressedWriter.Write(numentries);

  for(auto it = cache.begin(); it != cache.end(); ++it)
  {
    uint32_t hash = it->first;
    uint32_t len = callbacks.GetSize(it->second);
    const byte *data = callbacks.GetData(it->second);

    compressedWriter.Write(hash);
    compressedWriter.Write(len);
    compressedWriter.Write(data, len);

    callbacks.Destroy(it->second);
  }

  compressedWriter.Finish();

  RDCDEBUG("Successfully wrote %u entries to cache, compressed from %llu to %llu", numentries,
           uncompressedSize, fileWriter.GetOffset());
}
