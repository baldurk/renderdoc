/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "os/os_specific.h"

template <typename ResultType, typename ShaderCallbacks>
bool LoadShaderCache(const char *filename, const uint32_t magicNumber, const uint32_t versionNumber,
                     std::map<uint32_t, ResultType> &resultCache, const ShaderCallbacks &callbacks)
{
  std::string shadercache = FileIO::GetAppFolderFilename(filename);

  FILE *f = FileIO::fopen(shadercache.c_str(), "rb");

  if(!f)
    return false;

  FileIO::fseek64(f, 0, SEEK_END);
  uint64_t cachelen = FileIO::ftell64(f);
  FileIO::fseek64(f, 0, SEEK_SET);

  bool ret = true;

  // header: magic number, file version, number of entries
  if(cachelen < sizeof(uint32_t) * 3)
  {
    RDCERR("Invalid shader cache");
    ret = false;
  }
  else
  {
    byte *cache = new byte[(size_t)cachelen];
    FileIO::fread(cache, 1, (size_t)cachelen, f);

    uint32_t *header = (uint32_t *)cache;

    uint32_t fileMagic = header[0];
    uint32_t fileVer = header[1];

    if(fileMagic != magicNumber || fileVer != versionNumber)
    {
      RDCDEBUG("Out of date or invalid shader cache magic: %d version: %d", fileMagic, fileVer);
      ret = false;
    }
    else
    {
      uint32_t numentries = header[2];

      // assume at least 16 bytes for any cache entry. 8 bytes for hash and length, and 8 bytes
      // data.
      if(numentries > cachelen / 16LLU)
      {
        RDCERR("Invalid shader cache - more entries %u than are feasible in a %llu byte cache",
               numentries, cachelen);
        ret = false;
      }
      else
      {
        byte *ptr = cache + sizeof(uint32_t) * 3;

        int64_t bufsize = (int64_t)cachelen - sizeof(uint32_t) * 3;

        for(uint32_t i = 0; i < numentries; i++)
        {
          if((size_t)bufsize < sizeof(uint32_t))
          {
            RDCERR("Invalid shader cache - truncated, not enough data for shader hash");
            ret = false;
            break;
          }

          uint32_t hash = *(uint32_t *)ptr;
          ptr += sizeof(uint32_t);
          bufsize -= sizeof(uint32_t);

          if((size_t)bufsize < sizeof(uint32_t))
          {
            RDCERR("Invalid shader cache - truncated, not enough data for shader length");
            ret = false;
            break;
          }

          uint32_t len = *(uint32_t *)ptr;
          ptr += sizeof(uint32_t);
          bufsize -= sizeof(uint32_t);

          if(bufsize < len)
          {
            RDCERR("Invalid shader cache - truncated, not enough data for shader buffer");
            ret = false;
            break;
          }

          byte *data = ptr;
          ptr += len;
          bufsize -= len;

          ResultType result;
          bool created = callbacks.Create(len, data, &result);

          if(!created)
          {
            RDCERR("Couldn't create blob of size %u from shadercache", len);
            ret = false;
            break;
          }

          resultCache[hash] = result;
        }

        if(ret == true && bufsize != 0)
        {
          RDCERR("Invalid shader cache - trailing data");
          ret = false;
        }

        RDCDEBUG("Successfully loaded %d shaders from shader cache", resultCache.size());
      }
    }

    delete[] cache;
  }

  FileIO::fclose(f);

  return ret;
}

template <typename ResultType, typename ShaderCallbacks>
void SaveShaderCache(const char *filename, uint32_t magicNumber, uint32_t versionNumber,
                     const std::map<uint32_t, ResultType> &cache, const ShaderCallbacks &callbacks)
{
  std::string shadercache = FileIO::GetAppFolderFilename(filename);

  FILE *f = FileIO::fopen(shadercache.c_str(), "wb");

  if(!f)
  {
    RDCERR("Error opening shader cache for write");
    return;
  }

  FileIO::fwrite(&magicNumber, 1, sizeof(magicNumber), f);
  FileIO::fwrite(&versionNumber, 1, sizeof(versionNumber), f);
  uint32_t numentries = (uint32_t)cache.size();
  FileIO::fwrite(&numentries, 1, sizeof(numentries), f);

  for(auto it = cache.begin(); it != cache.end(); ++it)
  {
    uint32_t hash = it->first;
    uint32_t len = callbacks.GetSize(it->second);
    const byte *data = callbacks.GetData(it->second);
    FileIO::fwrite(&hash, 1, sizeof(hash), f);
    FileIO::fwrite(&len, 1, sizeof(len), f);
    FileIO::fwrite(data, 1, len, f);

    callbacks.Destroy(it->second);
  }

  FileIO::fclose(f);

  RDCDEBUG("Successfully wrote %u shaders to shader cache", numentries);
}
