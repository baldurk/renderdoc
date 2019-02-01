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

#include "lz4io.h"
#include "serialiser.h"
#include "zstdio.h"

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

TEST_CASE("Test LZ4 compression/decompression", "[streamio][lz4]")
{
  StreamWriter buf(StreamWriter::DefaultScratchSize);

  byte *randomData = new byte[1024 * 1024];

  for(int i = 0; i < 1024 * 1024; i++)
    randomData[i] = rand() & 0xff;

  // write the data
  {
    StreamWriter writer(new LZ4Compressor(&buf, Ownership::Nothing), Ownership::Stream);

    byte *fixedData = new byte[1024 * 1024];
    byte *regularData = new byte[1024 * 1024];

    memset(fixedData, 0x7c, 1024 * 1024);

    for(int i = 0; i < 1024 * 1024; i++)
      regularData[i] = i & 0xff;

    writer.Write(fixedData, 1024 * 1024);
    writer.Write(randomData, 1024 * 1024);
    writer.Write(regularData, 1024 * 1024);
    writer.Write(fixedData, 1024 * 1024);

    // check that the compression got good wins out of the above data. The random data will be
    // pretty much untouched but the rest should compress massively.
    CHECK(buf.GetOffset() < 1024 * 1024 + 20 * 1024);
    CHECK(writer.GetOffset() == 4 * 1024 * 1024);

    CHECK_FALSE(writer.IsErrored());

    writer.Finish();

    CHECK_FALSE(writer.IsErrored());

    delete[] fixedData;
    delete[] regularData;
  }

  // we now only have the compressed data, decompress it
  {
    StreamReader reader(
        new LZ4Decompressor(new StreamReader(buf.GetData(), buf.GetOffset()), Ownership::Stream),
        4 * 1024 * 1024, Ownership::Stream);
    // recreate this for easy memcmp'ing
    byte *fixedData = new byte[1024 * 1024];
    byte *regularData = new byte[1024 * 1024];

    memset(fixedData, 0x7c, 1024 * 1024);

    for(int i = 0; i < 1024 * 1024; i++)
      regularData[i] = i & 0xff;

    byte *readData = new byte[1024 * 1024];

    reader.Read(readData, 1024 * 1024);
    CHECK_FALSE(memcmp(readData, fixedData, 1024 * 1024));

    reader.Read(readData, 1024 * 1024);
    CHECK_FALSE(memcmp(readData, randomData, 1024 * 1024));

    reader.Read(readData, 1024 * 1024);
    CHECK_FALSE(memcmp(readData, regularData, 1024 * 1024));

    reader.Read(readData, 1024 * 1024);
    CHECK_FALSE(memcmp(readData, fixedData, 1024 * 1024));

    CHECK_FALSE(reader.IsErrored());
    CHECK(reader.AtEnd());

    delete[] readData;
    delete[] fixedData;
    delete[] regularData;
  }

  delete[] randomData;
};

TEST_CASE("Test ZSTD compression/decompression", "[streamio][zstd]")
{
  StreamWriter buf(StreamWriter::DefaultScratchSize);

  byte *randomData = new byte[1024 * 1024];

  for(int i = 0; i < 1024 * 1024; i++)
    randomData[i] = rand() & 0xff;

  // write the data
  {
    StreamWriter writer(new ZSTDCompressor(&buf, Ownership::Nothing), Ownership::Stream);

    byte *fixedData = new byte[1024 * 1024];
    byte *regularData = new byte[1024 * 1024];

    memset(fixedData, 0x7c, 1024 * 1024);

    for(int i = 0; i < 1024 * 1024; i++)
      regularData[i] = i & 0xff;

    writer.Write(fixedData, 1024 * 1024);
    writer.Write(randomData, 1024 * 1024);
    writer.Write(regularData, 1024 * 1024);
    writer.Write(fixedData, 1024 * 1024);

    // check that the compression got good wins out of the above data. The random data will be
    // pretty much untouched but the rest should compress massively.
    CHECK(buf.GetOffset() < 1024 * 1024 + 4 * 1024);
    CHECK(writer.GetOffset() == 4 * 1024 * 1024);

    CHECK_FALSE(writer.IsErrored());

    writer.Finish();

    CHECK_FALSE(writer.IsErrored());

    delete[] fixedData;
    delete[] regularData;
  }

  // we now only have the compressed data, decompress it
  {
    StreamReader reader(
        new ZSTDDecompressor(new StreamReader(buf.GetData(), buf.GetOffset()), Ownership::Stream),
        4 * 1024 * 1024, Ownership::Stream);
    // recreate this for easy memcmp'ing
    byte *fixedData = new byte[1024 * 1024];
    byte *regularData = new byte[1024 * 1024];

    memset(fixedData, 0x7c, 1024 * 1024);

    for(int i = 0; i < 1024 * 1024; i++)
      regularData[i] = i & 0xff;

    byte *readData = new byte[1024 * 1024];

    reader.Read(readData, 1024 * 1024);
    CHECK_FALSE(memcmp(readData, fixedData, 1024 * 1024));

    reader.Read(readData, 1024 * 1024);
    CHECK_FALSE(memcmp(readData, randomData, 1024 * 1024));

    reader.Read(readData, 1024 * 1024);
    CHECK_FALSE(memcmp(readData, regularData, 1024 * 1024));

    reader.Read(readData, 1024 * 1024);
    CHECK_FALSE(memcmp(readData, fixedData, 1024 * 1024));

    CHECK_FALSE(reader.IsErrored());
    CHECK(reader.AtEnd());

    delete[] readData;
    delete[] fixedData;
    delete[] regularData;
  }

  delete[] randomData;
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
