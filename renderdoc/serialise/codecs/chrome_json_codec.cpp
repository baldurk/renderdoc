/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include <utility>
#include "common/common.h"
#include "serialise/rdcfile.h"

ReplayStatus exportChrome(const char *filename, const RDCFile &rdc, const SDFile &structData,
                          RENDERDOC_ProgressCallback progress)
{
  FILE *f = FileIO::fopen(filename, "w");

  if(!f)
    return ReplayStatus::FileIOFailed;

  std::string str;

  // add header, customise this as needed.
  str = R"({
  "displayTimeUnit": "ns",
  "traceEvents": [)";

  const char *category = "Initialisation";

  // stupid JSON not allowing trailing ,s :(
  bool first = true;

  int i = 0;
  int numChunks = structData.chunks.count();

  for(const SDChunk *chunk : structData.chunks)
  {
    if(chunk->metadata.chunkID == (uint32_t)SystemChunk::FirstDriverChunk + 1)
      category = "Frame Capture";

    if(!first)
      str += ",";

    first = false;

    const char *fmt = R"(
    { "name": "%s", "cat": "%s", "ph": "B", "ts": %llu, "pid": 5, "tid": %u },
    { "ph": "E", "ts": %llu, "pid": 5, "tid": %u })";

    if(chunk->metadata.durationMicro == 0)
    {
      fmt = R"(
    { "name": "%s", "cat": "%s", "ph": "i", "ts": %llu, "pid": 5, "tid": %u })";
    }

    str += StringFormat::Fmt(
        fmt, chunk->name.c_str(), category, chunk->metadata.timestampMicro, chunk->metadata.threadID,
        chunk->metadata.timestampMicro + chunk->metadata.durationMicro, chunk->metadata.threadID);

    if(progress)
      progress(float(i) / float(numChunks));

    i++;
  }

  if(progress)
    progress(1.0f);

  // end trace events
  str += "\n  ]\n}";

  FileIO::fwrite(str.data(), 1, str.size(), f);

  FileIO::fclose(f);

  return ReplayStatus::Succeeded;
}

static ConversionRegistration XMLConversionRegistration(
    &exportChrome,
    {
        "chrome.json", "Chrome profiler JSON",
        R"(Exports the chunk threadID, timestamp and duration data to a JSON format that can be loaded
by chrome's profiler at chrome://tracing)",
        false,
    });